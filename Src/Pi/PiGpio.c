#include <stdlib.h>
#include <stdio.h>
#include "Led.h"

#define CLOCK 3
#define LATCH 4
#define DATA  25

#define POWER 0x80
#define FDD0  0x40
#define FDD1  0x20

static int ledBitMap = 0;

#ifdef __arm__
#include <wiringPi.h>
#include <wiringShift.h>

static void gpioShiftLeds();
void gpioInit()
{
    wiringPiSetup();
    pinMode(CLOCK, OUTPUT);
    pinMode(LATCH, OUTPUT);
    pinMode(DATA, OUTPUT);
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
    if (ledGetFdd1()) ledBitMap |= FDD0; else ledBitMap &= ~FDD0;
    if (ledGetFdd2()) ledBitMap |= FDD1; else ledBitMap &= ~FDD1;
    if (oldBitMap != ledBitMap) gpioShiftLeds();
}
static void gpioShiftLeds()
{
    digitalWrite(LATCH, LOW);
    shiftOut(DATA, CLOCK, LSBFIRST, ledBitMap);
    digitalWrite(LATCH, HIGH);
}
#else
void gpioInit() {}
void gpioShutdown() {}
void gpioUpdateLeds() {}
#endif
