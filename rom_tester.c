/*
 ** Standalone ROM Tester for RPMC (Raspberry Pi MSX Core)
 ** Reuses MsxBusPi.c GPIO/bus logic. Run via SSH.
 */
#define _GNU_SOURCE
#define ROM_TESTER_BUILD

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include <sched.h>
#include <errno.h>
#include <inttypes.h>

/* CRC-32 (IEEE) for ROM validation */
static uint32_t crc32_table[256];
static bool crc32_table_built = false;

static void crc32_build_table(void) {
    if (crc32_table_built) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c >> 1) ^ (0xEDB88320U & -(c & 1));
        crc32_table[i] = c;
    }
    crc32_table_built = true;
}

static uint32_t crc32_update(uint32_t crc, uint8_t b) {
    return crc32_table[(crc ^ b) & 0xFF] ^ (crc >> 8);
}

/* Match the signature MsxBusPi.c expects (single int arg) */
static void boardSetInt(int flag) { (void)flag; }

static uint32_t g_crc = 0xFFFFFFFF;
static FILE  *g_fp = NULL;
static uint8_t read_buf[0x4000];

static void crc_accumulate(uint8_t b) {
    g_crc = crc32_update(g_crc, b);
    if (g_fp) {
        uint8_t cpy = b;
        fwrite(&cpy, 1, 1, g_fp);
        fflush(g_fp);
    }
}

static void usage(const char *prog) {
    printf("Usage: %s [OPTIONS]\n", prog);
    printf("\n");
    printf("Standalone ROM dumper for RPMC (Raspberry Pi MSX Core).\n");
    printf("Reads physical MSX cartridges from the RPMC board slots.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -f FILE    Write ROM dump to FILE (default: stdout only)\n");
    printf("  -o OFFSET  Memory offset to start reading (default: 0x4000)\n");
    printf("  -s SIZE    Number of bytes to dump (default: 0x8000 = 32 KB)\n");
    printf("  -S SLOT    MSX parent slot (0 or 1, default: 0)\n");
    printf("             Slot 0 -> board connector 1 (SLTSL1 / MSX slot 1)\n");
    printf("             Slot 1 -> board connector 2 (SLTSL3 / MSX slot 3)\n");
    printf("  --io       Dump all 256 I/O port values instead of ROM\n");
    printf("  -h, --help Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s -f game.rom              Dump slot 0 ROM to game.rom\n", prog);
    printf("  %s -f upper.rom -S 1        Dump slot 1 ROM to upper.rom\n", prog);
    printf("  %s -f small.rom -s 0x4000   Dump only 16 KB\n", prog);
    printf("  %s --io                     Read and display all I/O ports\n", prog);
    printf("\n");
    printf("Notes:\n");
    printf("  - Must run as root (or with /dev/mem and bcm2835 access).\n");
    printf("  - Offset and size defaults read a standard 32 KB ROM.\n");
    printf("  - Auto-probing detects which sub-slot contains ROM data.\n");
}

/* Include the GPIO/bus driver */
#include "MsxBusPi.c"

int main(int argc, char **argv) {
    int io_mode = 0;
    int offset = 0x4000, size = 0x8000, slot = 0;
    int i, addr, c, sslot, found, page;
    uint8_t byte, test;
    struct timespec t1, t2;
    const char *outfile = NULL;
    int pos_offset = 0, pos_size = 0, pos_slot = 0;

    /* CLI parsing — supports both new flags and legacy positional args */
    for (int a = 1; a < argc; a++) {
        if (strcmp(argv[a], "-h") == 0 || strcmp(argv[a], "--help") == 0) {
            usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[a], "-f") == 0 && a + 1 < argc) {
            a++;
            outfile = argv[a];
        } else if (strcmp(argv[a], "-o") == 0 && a + 1 < argc) {
            a++;
            offset = strtoul(argv[a], NULL, 0);
            pos_offset = 1;
        } else if (strcmp(argv[a], "-s") == 0 && a + 1 < argc) {
            a++;
            size = (int)strtoul(argv[a], NULL, 0);
            pos_size = 1;
        } else if (strcmp(argv[a], "-S") == 0 && a + 1 < argc) {
            a++;
            slot = strtoul(argv[a], NULL, 0);
            pos_slot = 1;
        } else if (strcmp(argv[a], "--io") == 0) {
            io_mode = 1;
        } else {
            /* Legacy positional fallback: file, offset, size, slot */
            if (!outfile) {
                if (strcmp(argv[a], "io") == 0) {
                    io_mode = 1;
                } else {
                    outfile = argv[a];
                }
            } else if (!pos_offset) {
                offset = strtoul(argv[a], NULL, 0);
                pos_offset = 1;
            } else if (!pos_size) {
                size = (int)strtoul(argv[a], NULL, 0);
                pos_size = 1;
            } else if (!pos_slot) {
                slot = strtoul(argv[a], NULL, 0);
                pos_slot = 1;
            }
        }
    }

    if (outfile) {
        g_fp = fopen(outfile, "wb");
        if (!g_fp) {
            fprintf(stderr, "ERROR: cannot open output file '%s': %s\n", outfile, strerror(errno));
            return 1;
        }
    }

    /* Real-time priority */
    const struct sched_param param = {.sched_priority = 1};
    if (sched_setscheduler(0, SCHED_FIFO, &param) == -1)
        fprintf(stderr, "Warning: cannot set RT priority: %s\n", strerror(errno));

    /* Bus init */
    msxinit();
    crc32_build_table();
    g_crc = 0xFFFFFFFF;
    clock_gettime(CLOCK_MONOTONIC, &t1);

    /* IO port dump mode */
    if (io_mode) {
        for (i = 0; i < 256; i++) {
            if (i % 16 == 0) printf("%02x: ", i);
            printf("%02x ", msxreadio(i));
            if (i > 0 && i % 16 == 15) printf("\n");
        }
        goto done;
    }

    /* Resolve output path early */
    if (outfile && g_fp) {
        char abspath[1024];
        if (realpath(outfile, abspath)) {
            fclose(g_fp);
            g_fp = fopen(abspath, "wb");
            if (!g_fp) { fprintf(stderr, "ERROR: cannot open %s: %s\n", abspath, strerror(errno)); exit(1); }
        }
    }

    /*
     * MSX slot selection uses memory bus writes:
     *   0x6000 + (slot<<2) -> sub-slot selection
     *   0x6004 + (slot<<2) -> page selection
     *
     * For RPMC, msxwrite() is the correct path since the CPLD routes
     * slot/page controls through the memory bus, not I/O bus.
     */

    /* First, probe all 4 sub-slots to find which one has ROM data */
    printf("Probing sub-slots for parent slot %d...\n", slot);
    found = 0;
    sslot = 0;
    for (i = 0; i < 4; i++) {
        msxwrite(slot, 0x6000, i);
        test = msxread(slot, 0x4000);
        if (test != 0xFF) {
            msxwrite(slot, 0x6000, i);
            test = msxread(slot, 0x4000);
            if (test != 0xFF) {
                sslot = i;
                printf("  sub-slot %d has data (byte @0x4000 = 0x%02x)\n", i, test);
                found = 1;
                break;
            }
        }
    }

    if (!found) {
        printf("No ROM data found in any sub-slot of parent slot %d\n", slot);
        printf("\n");
        goto done;
    }

    /*
     * Pages 0-3 are already mapped to 0x4000-0x7FFF.
     * We map pages 4-7 to 0x6000-0x7FFF for addresses above 0xBFFF.
     */
    page = 4;
    for (addr = offset; addr < offset + size; addr++) {
        if (addr > 0xbfff) {
            if (!(addr & 0x1fff)) {
                msxwrite(slot, 0x6000, sslot);
                msxwrite(slot, 0x6004, page++);
                printf("\npage:%d  address=0x%04x\n", page - 1, addr);
            }
            byte = msxread(slot, 0x6000 + (addr & 0x1fff));
        } else {
            byte = msxread(slot, addr);
        }

        /* Repeat-read integrity check */
        c = 0;
        for (i = 0; i < 10; i++) {
            uint8_t byte0 = msxread(slot, (addr > 0xbfff) ? (0x6000 + (addr & 0x1fff)) : addr);
            if (byte != byte0) { c = 1; break; }
        }

        if (addr % 16 == 0) printf("\n%04x: ", addr);
        if (c)
            printf("\33[31m%02x \33[0m", byte);
        else
            printf("%02x ", byte);

        crc_accumulate(byte);
    }

done:
    clock_gettime(CLOCK_MONOTONIC, &t2);
    printf("\n");
    double elapsed = (t2.tv_sec - t1.tv_sec) * 1000.0
                   + (t2.tv_nsec - t1.tv_nsec) / 1e6;
    printf("ROM dump: %d bytes  elapsed: %.2f ms\n", size, elapsed);
    printf("CRC32: 0x%08"PRIX32"\n", g_crc ^ 0xFFFFFFFF);

    if (g_fp) fclose(g_fp);
    msxclose();
    return 0;
}
