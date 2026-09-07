#define _GNU_SOURCE
#define ROM_TESTER_BUILD
static void boardSetInt(int flag) { (void)flag; }
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "Src/IoDevice/MsxBusPi.c"

static unsigned char r8(unsigned slot, unsigned a){ return (unsigned char)msxread(slot, a); }
static void w8p(unsigned slot, unsigned a, unsigned char v)
{
    int cs1 = (a & 0xc000) == 0x4000 ? MSX_CS1 : 0;
    int cs2 = (a & 0xc000) == 0x8000 ? MSX_CS2 : 0;
    SetAddress(a);
    SetData(MSX_MREQ, (slot == 0 ? MSX_SLTSL1 : MSX_SLTSL3) | MSX_MREQ | cs1 | cs2, 45, v);
}

static unsigned char base[4][4];
static void grabBase(unsigned slot)
{
    for (int p = 0; p < 4; p++)
        for (int i = 0; i < 4; i++)
            base[p][i] = r8(slot, 0x4000 + p * 0x2000 + i);
}

/* check all 4 windows; return window index that changed vs base, else -1 */
static int windowsChanged(unsigned slot, unsigned char* out /*[4]*/)
{
    for (int p = 0; p < 4; p++) {
        unsigned b = 0x4000 + p * 0x2000;
        int diff = 0;
        unsigned char bytes[4];
        for (int i = 0; i < 4; i++) bytes[i] = r8(slot, b + i);
        for (int i = 0; i < 4; i++) if (bytes[i] != base[p][i]) diff = 1;
        if (diff) { memcpy(out, bytes, 4); return p; }
    }
    return -1;
}

int main(int argc, char** argv)
{
    int slot = argc > 1 ? atoi(argv[1]) : 0;
    struct sched_param prm = {.sched_priority=1};
    sched_setscheduler(0, SCHED_FIFO, &prm);
    msxinit();

    printf("=== slot %d baseline ===\n", slot);
    for (int p = 0; p < 4; p++) {
        unsigned b = 0x4000 + p * 0x2000;
        printf("%04x: ", b);
        for (int i = 0; i < 4; i++) printf("%02x ", r8(slot, b + i));
        printf("\n");
    }
    grabBase(slot);

    /* ---- test A: standard Konami5 with SCC enabled first ---- */
    printf("\n=== A) SCC-enable-first gating (Konami5 regs) ===\n");
    w8p(slot, 0x9000, 0x3f);
    {
        unsigned regs[4] = {0x5000, 0x7000, 0x9000, 0xB000};
        unsigned vals[3] = {1, 3, 0x1F};
        for (int r = 0; r < 4; r++) {
            for (int vi = 0; vi < 3; vi++) {
                unsigned v = vals[vi];
                w8p(slot, regs[r], (unsigned char)v);
                unsigned char nb[4];
                int p = windowsChanged(slot, nb);
                if (p >= 0)
                    printf("  SCC+reg %04x v=%02x -> window %04x = %02x %02x %02x %02x\n",
                           regs[r], v, 0x4000 + p * 0x2000, nb[0], nb[1], nb[2], nb[3]);
                else
                    printf("  SCC+reg %04x v=%02x -> unchanged\n", regs[r], v);
                grabBase(slot);
            }
        }
    }

    /* ---- test B: exhaustive register sweep, SCC re-enabled before each probe ---- */
    printf("\n=== B) exhaustive sweep 0x4000..0xBFFF step 0x10 ===\n");
    int hits = 0;
    for (unsigned a = 0x4000; a < 0xC000; a += 0x10) {
        w8p(slot, 0x9000, 0x3f);   /* ensure SCC mode */
        unsigned val = ((a >> 4) & 0x1F);   /* data varies with address to break ties */
        w8p(slot, a, (unsigned char)val);
        unsigned char nb[4];
        int p = windowsChanged(slot, nb);
        if (p >= 0) {
            printf("  reg %04x v=%02x -> window %04x = %02x %02x %02x %02x\n",
                   a, val, 0x4000 + p * 0x2000, nb[0], nb[1], nb[2], nb[3]);
            hits++;
            if (hits > 40) { printf("  (stopping after 40 hits)\n"); break; }
            grabBase(slot);
        }
    }
    if (!hits) printf("  no register in 0x4000-0xBFFF changed any window (SCC was enabled)\n");

    /* ---- test C: same sweep WITHOUT re-enabling SCC each time ---- */
    printf("\n=== C) exhaustive sweep, no SCC re-enable ===\n");
    grabBase(slot);
    hits = 0;
    for (unsigned a = 0x4000; a < 0xC000; a += 0x10) {
        unsigned char val = ((a >> 4) & 0x1F);
        w8p(slot, a, (unsigned char)val);
        unsigned char nb[4];
        int p = windowsChanged(slot, nb);
        if (p >= 0) {
            printf("  reg %04x v=%02x -> window %04x = %02x %02x %02x %02x\n",
                   a, val, 0x4000 + p * 0x2000, nb[0], nb[1], nb[2], nb[3]);
            hits++;
            if (hits > 40) { printf("  (stopping after 40 hits)\n"); break; }
            grabBase(slot);
        }
    }
    if (!hits) printf("  no register changed any window (no SCC re-enable)\n");

    printf("\ndone.\n");
    (void)r8;
    return 0;
}