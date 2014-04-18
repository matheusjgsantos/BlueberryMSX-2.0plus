/*****************************************************************************
** $Source: /cygdrive/d/Private/_SVNROOT/bluemsx/blueMSX/Src/IoDevice/Microchip24x00.h,v $
**
** $Revision: 1.3 $
**
** $Date: 2008-03-30 18:38:40 $
**
** More info: http://www.bluemsx.com
**
** Copyright (C) 2003-2012 Daniel Vik
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
#ifndef MICROCHIP_24x00_H
#define MICROCHIP_24x00_H

#include "MsxTypes.h"

typedef struct Microchip24x00 Microchip24x00;

Microchip24x00* microchip24x00Create(int size, const char* sramFilename);
void microchip24x00Destroy(Microchip24x00* rm);

void microchip24x00Reset(Microchip24x00* rm);

void microchip24x00SetScl(Microchip24x00* rm, int value);
void microchip24x00SetSda(Microchip24x00* rm, int value);
int microchip24x00GetSda(Microchip24x00* rm);
int microchip24x00GetScl(Microchip24x00* rm);

void microchip24x00SaveState(Microchip24x00* rm);
void microchip24x00LoadState(Microchip24x00* rm);

#endif

