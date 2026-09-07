#define _GNU_SOURCE
#define ROM_TESTER_BUILD
static void boardSetInt(int flag) { (void)flag; }
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "Src/IoDevice/MsxBusPi.c"

static int slot;
static unsigned char r8(unsigned a) { return (unsigned char)msxread(slot, a); }

/* write method 1: current SetData (what emulator/rom_tester use) */
static void w1(unsigned a, unsigned char v)
{
    SetAddress(a);
    SetData(MSX_MREQ, (slot == 0 ? MSX_SLTSL1 : MSX_SLTSL3) | MSX_MREQ, 45, v);
}
/* write method 2: clean single-pulse strobe, long WR-low */
static void w2(unsigned a, unsigned char v)
{
    int flag = (slot == 0 ? MSX_SLTSL1 : MSX_SLTSL3) | MSX_MREQ;
    SetAddress(a);
    GPIO_CLR = LE_C | 0xffff;          /* data bus out */
    GPIO_SET = v;                      /* drive data */
    GPIO_CLR = flag;                   /* assert SLTSL+MREQ once */
    SetDelay(200);                     /* settle */
    GPIO_CLR = MSX_WR;                 /* WR assert */
    SetDelay(50);                      /* write pulse */
    GPIO_SET = MSX_WR;                 /* WR release */
    SetDelay(10);
    GPIO_SET = MSX_CONTROLS | MSX_WR;  /* idle all controls */
    GPIO_CLR = LE_C;
}
/* write method 3: current SetData but long delay (slow CPU-like) */
static void w3(unsigned a, unsigned char v)
{
    SetAddress(a);
    SetData(MSX_MREQ, (slot == 0 ? MSX_SLTSL1 : MSX_SLTSL3) | MSX_MREQ, 8000, v);
}

static void (*W)(unsigned, unsigned char);

static void show(char* tag)
{
    printf("%-22s 7FFC=%02x 7FFD=%02x 7FFE=%02x 7FFF=%02x | "
           "4000:%02x%02x 6000:%02x%02x 8000:%02x%02x A000:%02x%02x\n",
           tag,
           r8(0x7FFC), r8(0x7FFD), r8(0x7FFE), r8(0x7FFF),
           r8(0x4000), r8(0x4001), r8(0x6000), r8(0x6001),
           r8(0x8000), r8(0x8001), r8(0xA000), r8(0xA001));
}

int main(int argc, char** argv)
{
    slot = argc > 1 ? atoi(argv[1]) : 0;
    struct sched_param prm = {.sched_priority = 1};
    sched_setscheduler(0, SCHED_FIFO, &prm);
    msxinit();

    printf("=== Yamanooto register probe, slot %d ===\n", slot);
    for (int m = 1; m <= 3; m++) {
        W = m == 1 ? w1 : (m == 2 ? w2 : w3);
        printf("\n--- write method %d ---\n", m);
        show("reset (as-is)");
        W(0x7FFF, 0x01);              /* ENAR: enable register read */
        show("after ENAR=1");
        W(0x7FFE, 0x05);              /* OFFR = 5 */
        show("after OFFR=5");
        W(0x7FFD, 0x00);              /* CFGR: MDIS=0, K4=0 ... */
        show("after CFGR=0");
        W(0x9000, 0x10);              /* K5 bank2 = 16 */
        show("after bank2=16");
        W(0x5000, 0x00); W(0x7000, 0x01); W(0xB000, 0x03); /* reset banks */
        show("banks 0,1,3 reset");
        unsigned saved = r8(0x8000);
        W(0x7FFF, 0x12);              /* ENAR with WREN bit4 -> flash write?? */
        show("after ENAR=0x12(WREN)");
        W(0x7FFF, 0x00);              /* disable regs again */
        show("after ENAR=0");
        (void)saved;
    }
    printf("\ndone.\n");
    return 0;
}