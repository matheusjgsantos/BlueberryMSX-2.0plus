/*****************************************************************************
**
** blueberryMSX
** https://github.com/Melllvar/blueberryMSX
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

#include "Led.h"

#ifdef RASPI_GPIO

#define POWER_LED   0 // #17
#define FLOPPY1_LED 2 // 1:21-2:27
#define FLOPPY2_LED 3 // #22

void gpioInit()
{
	wiringPiSetup();
	pinMode(POWER_LED, OUTPUT) ;

	digitalWrite(POWER_LED,   LOW);
	digitalWrite(FLOPPY1_LED, LOW);
	digitalWrite(FLOPPY2_LED, LOW);
}

void gpioTogglePowerLed(int on)
{
	digitalWrite(POWER_LED, on ? HIGH : LOW);
}

void gpioToggleFloppyLed(int floppy, int on)
{
	digitalWrite(floppy == 0 ? FLOPPY1_LED : FLOPPY2_LED, on ? HIGH : LOW);
}

#endif
