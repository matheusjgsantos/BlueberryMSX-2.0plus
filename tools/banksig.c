#define _GNU_SOURCE
#define ROM_TESTER_BUILD
static void boardSetInt(int flag) { (void)flag; }
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "Src/IoDevice/MsxBusPi.c"

#define SIGLEN 64
static uint8_t base[4][SIGLEN];
static void grab_base(int slot)
{
    for (int p = 0; p < 4; p++)
        for (int i = 0; i < SIGLEN; i++)
            base[p][i] = (uint8_t)msxread(slot, 0x4000 + p * 0x2000 + i);
}
static void w8(int slot, unsigned a, unsigned char v)
{
    SetAddress(a);
    SetData(MSX_MREQ, (slot == 0 ? MSX_SLTSL1 : MSX_SLTSL3) | MSX_MREQ, 45, v);
}

/* returns -1 if window p unchanged, else first differing byte index+1 */
static int changed(int slot, int p)
{
    for (int i = 0; i < SIGLEN; i++) {
        if ((uint8_t)msxread(slot, 0x4000 + p * 0x2000 + i) != base[p][i]) return i + 1;
    }
    return -1;
}

static void probe_set(int slot, const char* label, const unsigned* addrs, int na)
{
    static const uint8_t vals[8] = {0x01, 0x02, 0x03, 0x05, 0x1F, 0x20, 0x3F, 0x7F};
    for (int a = 0; a < na; a++) {
        for (int v = 0; v < 8; v++) {
            grab_base(slot);
            w8(slot, addrs[a], vals[v]);
            for (int p = 0; p < 4; p++) {
                int d = changed(slot, p);
                if (d > 0) {
                    printf("%-10s %04x v=%02x -> W%d first-diff@%d now=%02x %02x %02x %02x\n",
                           label, addrs[a], vals[v], p, d - 1,
                           (uint8_t)msxread(slot, 0x4000 + p * 0x2000),
                           (uint8_t)msxread(slot, 0x4000 + p * 0x2000 + 1),
                           (uint8_t)msxread(slot, 0x4000 + p * 0x2000 + 2),
                           (uint8_t)msxread(slot, 0x4000 + p * 0x2000 + 3));
                    return; /* one hit per register is enough */
                }
            }
        }
    }
    printf("%-10s no response (64-byte sig)\n", label);
}

int main(int argc, char** argv)
{
    int slot = argc > 1 ? atoi(argv[1]) : 0;
    struct sched_param prm = {.sched_priority = 1};
    sched_setscheduler(0, SCHED_FIFO, &prm);
    msxinit();

    printf("=== slot %d 64-byte signature sweeps ===\n", slot);
    grab_base(slot);
    printf("base 4000: "); for (int i = 0; i < 8; i++) printf("%02x ", (uint8_t)msxread(slot, 0x4000 + i)); printf("\n");
    printf("base 6000: "); for (int i = 0; i < 8; i++) printf("%02x ", (uint8_t)msxread(slot, 0x6000 + i)); printf("\n");

    unsigned k5[4]  = {0x5000, 0x7000, 0x9000, 0xB000};
    unsigned k4[3]  = {0x6000, 0x8000, 0xA000};
    unsigned sccp[4]= {0x7FF5, 0x7FF6, 0x7FF7, 0x7FF8};
    unsigned a8[4]  = {0x6000, 0x6800, 0x7000, 0x7800};
    unsigned a16[2] = {0x6000, 0x7000};
    unsigned misc[6]= {0x4000, 0x7FF0, 0x7FFE, 0xBFFF, 0xBFFE, 0x1FF6};

    probe_set(slot, "Konami5", k5, 4);
    probe_set(slot, "Konami4", k4, 3);
    probe_set(slot, "SCC+7FFx", sccp, 4);
    probe_set(slot, "Ascii8", a8, 4);
    probe_set(slot, "Ascii16", a16, 2);
    probe_set(slot, "misc", misc, 6);

    printf("done.\n");
    return 0;
}