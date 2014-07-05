/*****************************************************************************
**
** blueberryMSX
** https://github.com/pokebyte/blueberryMSX
**
** An MSX Emulator for Raspberry Pi based on blueMSX
**
** Copyright (C) 2014 Akop Karapetyan
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
******************************************************************************
*/

#include <stdlib.h>
#include <stdio.h>

#include <wiringPi.h>
#include <wiringShift.h>

#include "Led.h"

#ifdef RASPI_GPIO

#define CLOCK 1
#define LATCH 0
#define DATA  3

#define POWER 0x80
#define FDD0  0x40
#define FDD1  0x20

static int ledBitMap = 0;

static void gpioShiftLeds();

void gpioInit()
{
	wiringPiSetup();

	pinMode(CLOCK, OUTPUT) ;
	pinMode(LATCH, OUTPUT) ;
	pinMode(DATA,  OUTPUT) ;

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

	if (ledGetFdd1()) {
		ledBitMap |= FDD0;
	} else {
		ledBitMap &= ~FDD0;
	}

	if (ledGetFdd2()) {
		ledBitMap |= FDD1;
	} else {
		ledBitMap &= ~FDD1;
	}

	if (oldBitMap != ledBitMap) {
		gpioShiftLeds();
	}
}

static void gpioShiftLeds()
{
	digitalWrite(LATCH, LOW);
	shiftOut(DATA, CLOCK, LSBFIRST, ledBitMap);
	digitalWrite(LATCH, HIGH);
}

#endif
