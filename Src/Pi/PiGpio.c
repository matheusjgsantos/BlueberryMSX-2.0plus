#include <stdlib.h>
#include <stdio.h>
#include "Led.h"

// 74HC595 shift-register pins in BCM/GPIO numbering, converted from the
// original wiringPi numbers: CLOCK 3->22, LATCH 4->23, DATA 25->26.
#define CLOCK 22
#define LATCH 23
#define DATA  26

#define POWER 0x80
#define SLT2  0x40
#define SLT1  0x20
#define IO    0x10
#define HAN   0x08
#define CAPS  0x04

static int ledBitMap = 0;

#if defined(__arm__) || defined(__aarch64__)
#include <bcm2835.h>

static void gpioShiftLeds();
void gpioInit()
{
    if (!bcm2835_init())
    {
        fprintf(stderr, "PiGpio: bcm2835_init() failed - slot LEDs disabled\n");
        return;
    }
    bcm2835_gpio_fsel(CLOCK, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(LATCH, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(DATA,  BCM2835_GPIO_FSEL_OUTP);
    ledBitMap = POWER;
    gpioShiftLeds();
}
void gpioShutdown()
{
    ledBitMap = 0;
    gpioShiftLeds();
}
void gpioUpdateLeds()
{
    int oldBitMap = ledBitMap;
    if (ledGetSlot2Busy()) ledBitMap |= SLT2; else ledBitMap &= ~SLT2;
    if (ledGetSlot1Busy()) ledBitMap |= SLT1; else ledBitMap &= ~SLT1;
    if (ledGetSlot1Busy() || ledGetSlot2Busy()) ledBitMap |= IO; else ledBitMap &= ~IO;
    if (ledGetKana()) ledBitMap |= HAN; else ledBitMap &= ~HAN;
    if (ledGetCapslock()) ledBitMap |= CAPS; else ledBitMap &= ~CAPS;
    if (oldBitMap != ledBitMap) gpioShiftLeds();
}
static void gpioShiftLeds()
{
    bcm2835_gpio_write(LATCH, 0);
    for (int bit = 0; bit < 8; bit++)
    {
        bcm2835_gpio_write(DATA, (ledBitMap >> bit) & 1);
        bcm2835_gpio_write(CLOCK, 0);
        bcm2835_delayMicroseconds(10);
        bcm2835_gpio_write(CLOCK, 1);
        bcm2835_delayMicroseconds(10);
    }
    bcm2835_gpio_write(LATCH, 1);
    bcm2835_delayMicroseconds(10);
}
#else
void gpioInit() {}
void gpioShutdown() {}
void gpioUpdateLeds() {}
#endif
