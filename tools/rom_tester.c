/*
 ** Standalone ROM Tester for RPMC (Raspberry Pi MSX Core)
 ** Reuses MsxBusPi.c GPIO/bus logic. Run via SSH.
 **
 ** Identifies the cartridge mapper by probing known register addresses and
 ** dumps the full image using the correct bank-switching protocol:
 **
 **   Konami4   : regs 0x6000/0x8000/0xA000, 8KB banks, page0 fixed
 **   Konami5   : regs 0x5000/0x7000/0x9000/0xB000, 8KB banks, SCC at 0x9800
 **   ASCII8    : regs 0x6000/0x6800/0x7000/0x7800, 8KB banks
 **   ASCII16   : regs 0x6000/0x7000, 16KB banks
 **   plain     : no bank switching, linear dump of 0x4000-0xBFFF
 **
 ** Modes:
 **   rom_tester <file>              -> identify mapper and dump full cartridge
 **   rom_tester ident [-S slot]     -> identify only, no dump
 **   rom_tester io                   -> dump I/O ports
 **   rom_tester <file> [-o a] [-s n] [-S slot]  -> legacy linear dump
 **                                      (extract a..a+n from the bus)
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

static void crc_accumulate(uint8_t b) {
    g_crc = crc32_update(g_crc, b);
    if (g_fp) {
        uint8_t cpy = b;
        fwrite(&cpy, 1, 1, g_fp);
    }
}

/* Buffered emit for high-volume mapper dumps (no per-byte flush) */
static void emit(const uint8_t* p, size_t n) {
    for (size_t i = 0; i < n; i++) g_crc = crc32_update(g_crc, p[i]);
    if (g_fp) fwrite(p, 1, n, g_fp);
}

/* Include the GPIO/bus driver */
#include "MsxBusPi.c"

/* ------------------------------------------------------------------ */
/* Mapper identification                                               */
/* ------------------------------------------------------------------ */

#define MAP_UNKNOWN 0
#define MAP_PLAIN   1
#define MAP_KONAMI4 2
#define MAP_KONAMI5 3
#define MAP_ASCII8  4
#define MAP_ASCII16 5

static const char* mapper_name(int m) {
    switch (m) {
    case MAP_PLAIN:   return "linear (no bank switching)";
    case MAP_KONAMI4: return "Konami4  (0x6000/0x8000/0xA000, 8KB banks, page0 fixed)";
    case MAP_KONAMI5: return "Konami5 / KonamiSCC (0x5000/0x7000/0x9000/0xB000, 8KB, all pages)";
    case MAP_ASCII8:  return "ASCII8   (0x6000/0x6800/0x7000/0x7800, 8KB banks)";
    case MAP_ASCII16: return "ASCII16  (0x6000/0x7000, 16KB banks)";
    default:          return "unknown";
    }
}

static uint8_t r8(int slot, unsigned a) { return (uint8_t)msxread(slot, a); }

static void grab_base(int slot, uint8_t base[4][4]) {
    for (int p = 0; p < 4; p++)
        for (int i = 0; i < 4; i++)
            base[p][i] = r8(slot, 0x4000 + p * 0x2000 + i);
}

static bool win_diff(int slot, int win, const uint8_t base[4][4]) {
    unsigned b = 0x4000 + win * 0x2000;
    for (int i = 0; i < 4; i++)
        if (r8(slot, b + i) != base[win][i]) return true;
    return false;
}

/* Write `addr` with candidate bank values; return bitmap of windows that
** changed (bit w = window at 0x4000+0x2000*w), or 0 if none changed. */
static int probe_wr(int slot, unsigned addr) {
    static const uint8_t vals[5] = {0x02, 0x03, 0x05, 0x07, 0x1F};
    for (int vi = 0; vi < 5; vi++) {
        uint8_t base[4][4];
        grab_base(slot, base);
        msxwrite(slot, addr, vals[vi]);
        int bitmap = 0;
        for (int w = 0; w < 4; w++)
            if (win_diff(slot, w, base)) bitmap |= (1 << w);
        if (bitmap) return bitmap;
    }
    return 0;
}

static int detect_mapper(int slot) {
    int bm;

    /* Konami5: a write to 0x5000 (0x5000-0x57FF is a valid reg) moves W0 */
    bm = probe_wr(slot, 0x5000);
    if (bm & 1) return MAP_KONAMI5;

    /* 0x6000 discriminates the ASCII family from Konami4:
    **   ASCII16 -> W0 + W1 (16KB from 0x4000-0x7FFF)
    **   ASCII8  -> W0 only
    **   Konami4 -> W1 only */
    bm = probe_wr(slot, 0x6000);
    if (bm & 1) {
        if (bm & 2) return MAP_ASCII16;
        return MAP_ASCII8;
    }
    if (bm & 2) {
        /* W1 changed at 0x6000 -> Konami4 page1; confirm page2/page3 */
        if (probe_wr(slot, 0x8000) & 4) return MAP_KONAMI4;
        if (probe_wr(slot, 0xA000) & 8) return MAP_KONAMI4;
        if (probe_wr(slot, 0x6800) & 2) return MAP_ASCII8;
        return MAP_UNKNOWN;
    }
    /* late catches: ASCII8 on other pages, Konami4 page2-only */
    if (probe_wr(slot, 0x6800) & 2) return MAP_ASCII8;
    if (probe_wr(slot, 0x8000) & 4) return MAP_KONAMI4;
    return MAP_UNKNOWN;
}

/* SCC presence check for Konami5 carts: enable SCC (0x9000 = 0x3F),
** write a marker to the wave RAM (0x9800-0x980F) and read it back. */
static bool scc_present(int slot) {
    uint8_t pat[16], after[16];
    for (int i = 0; i < 16; i++) pat[i] = (uint8_t)(i * 2);
    msxwrite(slot, 0x9000, 0x3F);               /* SCC enable */
    for (int i = 0; i < 16; i++) msxwrite(slot, 0x9800 + i, pat[i]); /* wave RAM */
    for (int i = 0; i < 16; i++) after[i] = r8(slot, 0x9800 + i);
    msxwrite(slot, 0x9000, 0x00);               /* SCC disable, W2 back to bank 0 */
    return memcmp(after, pat, 16) == 0;
}

/* Number of banks: sweep bank values 0..63 on the selected page, record a
** signature per value, then pick the smallest power of two N such that the
** content wraps exactly: sig[B] == sig[B & (N-1)]. Falls back to the
** distinct-bank count rounded up to a power of two. */
static int count_banks(int slot, int mapper) {
    unsigned reg;
    unsigned base_addr;
    uint8_t sig[64][16];

    if (mapper == MAP_KONAMI5) { reg = 0x7000; base_addr = 0x6000; }
    else if (mapper == MAP_KONAMI4) { reg = 0x6000; base_addr = 0x6000; }
    else if (mapper == MAP_ASCII8)  { reg = 0x6800; base_addr = 0x6000; }
    else                            { reg = 0x6000; base_addr = 0x4000; } /* ASCII16 */

    for (int B = 0; B < 64; B++) {
        msxwrite(slot, reg, (uint8_t)B);
        for (int i = 0; i < 16; i++)
            sig[B][i] = r8(slot, base_addr + i);
    }

    int N = -1;
    for (int n = 1; n <= 64; n <<= 1) {
        bool ok = true;
        for (int B = 0; B < 64; B++) {
            if (memcmp(sig[B], sig[B & (n - 1)], 16) != 0) { ok = false; break; }
        }
        if (ok) { N = n; break; }
    }
    if (N < 0) {
        int distinct[64], dist = 0;
        for (int B = 0; B < 64; B++) {
            int dup = 0;
            for (int j = 0; j < dist; j++)
                if (memcmp(sig[B], sig[distinct[j]], 16) == 0) { dup = 1; break; }
            if (!dup) distinct[dist++] = B;
        }
        for (N = 1; N < dist; N <<= 1);
        if (N < 1) N = 1;
    }
    return N;
}

/* ------------------------------------------------------------------ */
/* Mapper-aware dumping                                                */
/* ------------------------------------------------------------------ */

#define W0 0x4000
#define W1 0x6000

static void dump_window(uint8_t* out, int slot, unsigned addr, int n) {
    for (int i = 0; i < n; i++) out[i] = r8(slot, addr + i);
    emit(out, (size_t)n);
}

int main(int argc, char **argv) {
    int     io_mode = 0, ident_mode = 0;
    int     offset = -1, size = -1, slot = 0;
    int     i, c, addr, page;
    uint8_t byte;
    struct  timespec t1, t2;

    /* CLI parsing: <arg1> is file, "io", "ident" or "-h";
    ** -S <slot>, -o <offset>, -s <size> may appear anywhere after. */
    if (argc > 1) {
        for (i = 1; i < argc; i++) {
            if (strcmp(argv[i], "io") == 0)       io_mode = 1;
            else if (strcmp(argv[i], "ident") == 0) ident_mode = 1;
            else if (strcmp(argv[i], "-h") == 0)  { io_mode = 2; break; }
            else if (strcmp(argv[i], "-S") == 0 && i + 1 < argc) slot = atoi(argv[++i]);
            else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) offset = (int)strtoul(argv[++i], NULL, 0);
            else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) size   = (int)strtoul(argv[++i], NULL, 0);
            else if (argv[i][0] != '-' && !g_fp)  {
                if (strcmp(argv[i], "io") && strcmp(argv[i], "ident"))
                    g_fp = fopen(argv[i], "wb");
            }
            else if (argv[i][0] != '-' && offset < 0) offset = (int)strtoul(argv[i], NULL, 0);
            else if (argv[i][0] != '-')               size   = (int)strtoul(argv[i], NULL, 0);
        }
    }

    if (io_mode == 2) {
        printf("usage:\n"
               "  %s <file>              full mapper-aware dump\n"
               "  %s ident [-S slot]     identify mapper only\n"
               "  %s io                  dump I/O ports\n"
               "  %s <file> [-o off] [-s n] [-S slot]   legacy linear dump\n",
               argv[0], argv[0], argv[0], argv[0]);
        return 0;
    }

    /* Real-time priority */
    const struct sched_param param = {.sched_priority = 1};
    if (sched_setscheduler(0, SCHED_FIFO, &param) == -1)
        fprintf(stderr, "Warning: cannot set RT priority: %s\n", strerror(errno));

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
    if (g_fp) {
        char abspath[1024];
        if (realpath(argv[1], abspath)) {
            fclose(g_fp);
            g_fp = fopen(abspath, "wb");
            if (!g_fp) { fprintf(stderr, "ERROR: cannot open %s\n", abspath); exit(1); }
        }
    }

    printf("Initializing slot %d via msxwrite(slot, 0x6000, 3)...\n", slot);
    msxwrite(slot, 0x6000, 3);

    byte = msxread(slot, 0x4000);
    if (byte == 0xFF && msxread(slot, 0x4001) == 0xFF && msxread(slot, 0x4002) == 0xFF) {
        printf("ERROR: slot %d returns 0xFF at 0x4000 - no cartridge responding\n", slot);
        goto done;
    }
    printf("  Slot %d responds: byte @0x4000 = 0x%02x\n", slot, byte);

    /* ---- explicit offset/size: legacy linear dump ---- */
    if (offset >= 0 && size > 0) {
        page = 4;
        for (addr = offset; addr < offset + size; addr++) {
            if (addr > 0xbfff) {
                if (!(addr & 0x1fff)) {
                    msxwrite(slot, 0x6000, page++);
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
        printf("\n");
        printf("ROM dump: %d bytes\n", size);
        printf("CRC32: 0x%08" PRIX32 "\n", g_crc ^ 0xFFFFFFFF);
        goto done;
    }

    /* ---- mapper identification ---- */
    int mapper = detect_mapper(slot);
    printf("\n===== CARTRIDGE IDENTIFICATION =====\n");
    printf("mapper    : %s\n", mapper_name(mapper));
    if (mapper == MAP_UNKNOWN) {
        printf("no known mapper register responded to writes;\n"
               "cart uses no bank switching (or its write path is dead).\n");
    }

    if (ident_mode) {
        printf("\nidentification complete (no dump requested).\n");
        goto done;
    }

    if (!g_fp) { printf("no output file given; aborting dump.\n"); goto done; }

    if (mapper == MAP_UNKNOWN) {
        /* plain / non-responsive cart: linear dump of the visible address space */
        uint8_t buf[0x8000];
        dump_window(buf, slot, 0x4000, 0x8000);
        printf("\nlinear dump of 0x4000-0xBFFF (%d bytes)\n", 0x8000);
    }
    else if (mapper == MAP_KONAMI4) {
        /* page0 (bank0 first half) is fixed; the rest comes from page1 */
        int banks = count_banks(slot, mapper);
        uint8_t buf[0x2000];
        dump_window(buf, slot, W0, 0x2000);               /* bank0 0x0000-0x1FFF */
        for (int B = 0; B < banks; B++) {
            msxwrite(slot, 0x6000, (uint8_t)B);           /* page1 bank = B */
            dump_window(buf, slot, W1, 0x2000);           /* bank B 8KB */
        }
        printf("\nKonami4 dump: %d banks x 8KB = %d bytes\n", banks, banks * 0x2000);
    }
    else if (mapper == MAP_KONAMI5) {
        int banks = count_banks(slot, mapper);
        bool scc = scc_present(slot);
        printf("scc       : %s\n", scc ? "present" : "absent");
        uint8_t buf[0x2000];
        for (int B = 0; B < banks; B++) {
            msxwrite(slot, 0x7000, (uint8_t)B);           /* page1 bank = B */
            dump_window(buf, slot, W1, 0x2000);           /* clean 8KB, no SCC overlay */
        }
        printf("\nKonami5 dump: %d banks x 8KB = %d bytes%s\n",
               banks, banks * 0x2000, scc ? " (SCC)" : "");
    }
    else if (mapper == MAP_ASCII8) {
        int banks = count_banks(slot, mapper);
        uint8_t buf[0x2000];
        for (int B = 0; B < banks; B++) {
            msxwrite(slot, 0x6800, (uint8_t)B);           /* page1 bank = B */
            dump_window(buf, slot, W1, 0x2000);
        }
        printf("\nASCII8 dump: %d banks x 8KB = %d bytes\n", banks, banks * 0x2000);
    }
    else { /* MAP_ASCII16 */
        int banks = count_banks(slot, mapper);
        uint8_t buf[0x4000];
        for (int B = 0; B < banks; B++) {
            msxwrite(slot, 0x6000, (uint8_t)B);           /* 16KB block B */
            dump_window(buf, slot, 0x4000, 0x4000);
        }
        printf("\nASCII16 dump: %d banks x 16KB = %d bytes\n", banks, banks * 0x4000);
    }

    printf("CRC32: 0x%08" PRIX32 "\n", g_crc ^ 0xFFFFFFFF);

done:
    clock_gettime(CLOCK_MONOTONIC, &t2);
    double elapsed = (t2.tv_sec - t1.tv_sec) * 1000.0
                   + (t2.tv_nsec - t1.tv_nsec) / 1e6;
    printf("elapsed: %.2f ms\n", elapsed);

    if (g_fp) fclose(g_fp);
    msxclose();
    return 0;
}