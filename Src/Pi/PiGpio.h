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

#ifndef PI_GPIO_H
#define PI_GPIO_H

#ifdef RASPI_GPIO

void gpioInit();
void gpioTogglePowerLed(int on);
void gpioToggleFloppyLed(int floppy, int on);

#endif // RASPI_GPIO

#endif // PI_GPIO_H
