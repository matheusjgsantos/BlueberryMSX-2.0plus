/*****************************************************************************
** $Source: /cygdrive/d/Private/_SVNROOT/bluemsx/blueMSX/Src/IoDevice/Microchip24x00.c,v $
**
** $Revision: 1.4 $
**
** $Date: 2008-03-30 18:38:40 $
**
** More info: http://www.bluemsx.com
**
** Copyright (C) 2003-2006 Daniel Vik
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
#include "Microchip24x00.h"
#include "Board.h"
#include "SaveState.h"
#include "sramLoader.h"
#include <stdlib.h>
#include <string.h>

typedef struct Microchip24x00
{
    UInt8* romData;
    int    romMask;
    
    int scl;
    int sda;
    int phase;
    int counter;
    int command;
    int address;
    int data;
    int dataStable;

    BoardTimer* timer;
    
    char sramFilename[512];
};

#define PHASE_IDLE     0
#define PHASE_COMMAND  1
#define PHASE_ADDRESS  2
#define PHASE_WRITE_1  3
#define PHASE_WRITE    4
#define PHASE_READ     5
#define PHASE_WRITING  6


static void onTimer(Microchip24x00* rm, UInt32 time)
{
    rm->phase = PHASE_IDLE;
}

Microchip24x00* microchip24x00Create(int size, const char* sramFilename)
{
    Microchip24x00* rm = calloc(1, sizeof(Microchip24x00));

    // Allocate memory
    rm->romMask = (size - 1) & 0xff;
    rm->romData = malloc(size);
    memset(rm->romData, 0xff, size);

    // Load rom data if present
    if (sramFilename != NULL) {
        strcpy(rm->sramFilename, sramFilename);
        sramLoad(rm->sramFilename, rm->romData, rm->romMask + 1, NULL, 0);
    }

    rm->timer = boardTimerCreate(onTimer, rm);

    microchip24x00Reset(rm);

    return rm;
}

void microchip24x00Destroy(Microchip24x00* rm)
{
    if (rm->sramFilename[0]) {
        sramSave(rm->sramFilename, rm->romData, rm->romMask + 1, NULL, 0);
    }

    boardTimerDestroy(rm->timer);

    free(rm->romData);
    free(rm);
}

void microchip24x00Reset(Microchip24x00* rm)
{
    rm->scl = 0;
    rm->sda = 0;
    rm->phase = PHASE_IDLE;
    rm->counter = 0;
    rm->command = 0;
    rm->address = 0;
    rm->data = 0;
    rm->dataStable = 0;
}

void microchip24x00SaveState(Microchip24x00* rm)
{
    SaveState* state = saveStateOpenForWrite("Microchip24x00");

    saveStateSet(state, "scl",           rm->scl);
    saveStateSet(state, "sda",           rm->sda);
    saveStateSet(state, "phase",         rm->phase);
    saveStateSet(state, "counter",       rm->counter);
    saveStateSet(state, "command",       rm->command);
    saveStateSet(state, "address",       rm->address);
    saveStateSet(state, "data",          rm->data);
    saveStateSet(state, "dataStable",    rm->dataStable);

    saveStateClose(state);
}

void microchip24x00LoadState(Microchip24x00* rm)
{
    SaveState* state = saveStateOpenForRead("Microchip24x00");

    rm->scl             = saveStateGet(state, "scl",          0);
    rm->sda             = saveStateGet(state, "sda",          0);
    rm->phase           = saveStateGet(state, "phase",        0);
    rm->counter         = saveStateGet(state, "counter",      0);
    rm->command         = saveStateGet(state, "command",      0);
    rm->address         = saveStateGet(state, "address",      0);
    rm->data            = saveStateGet(state, "data",         0);
    rm->dataStable      = saveStateGet(state, "dataStable",   0);

    saveStateClose(state);
}

void microchip24x00SetScl(Microchip24x00* rm, int value)
{
    int change;
    value = value ? 1 : 0;
    change = rm->scl ^ value;
    rm->scl = value;

    if (!change) {
        return;
    }

    if (value) {
        // Clock: LO -> HI
        if (rm->counter < 8) {
            if (rm->phase == PHASE_READ) {
                rm->sda = (rm->data >> 7) & 1;
                rm->data <<= 1;
            }
            rm->dataStable = 1;
            return;
        }

        rm->counter = 0;

        switch (rm->phase) {
        case PHASE_IDLE:
            // Return without ack as no transfer is in progress
        case PHASE_READ:
            // Return without ack as master acks
            break;
        case PHASE_WRITING:
            // If the eeprom is writing, return without ack
            break;
        case PHASE_COMMAND:
            rm->command = rm->data;
            if ((rm->command & 0xf0) == 0xa0) {
                if (rm->command & 1) {
                    rm->phase = PHASE_READ;
                    rm->data = rm->romData[rm->address];
                }
                else {
                    rm->phase = PHASE_ADDRESS;
                }
                rm->sda = 0;
            }
            else {
                rm->phase = PHASE_IDLE;
            }
            rm->sda = 0;
            break;
        case PHASE_ADDRESS:
            rm->address = rm->data & rm->romMask;
            rm->phase = PHASE_WRITE_1;
            rm->sda = 0;
            break;
        case PHASE_WRITE_1:
        case PHASE_WRITE:
            rm->phase = PHASE_WRITE;
            rm->sda = 0;
            break;
        }

        return;
    }

    // Clock: HI -> LO
    if (!rm->dataStable) {
        return;
    }

    rm->dataStable = 0;
    
    rm->counter++;

    if (rm->phase != PHASE_READ) {
        rm->data = (rm->data << 1) | rm->sda;
    }
    rm->data &= 0xff;
}

void microchip24x00SetSda(Microchip24x00* rm, int value)
{
    int change;
    value = value ? 1 : 0;
    change = rm->sda ^ value;
    rm->sda = value;
    if (change) {
        rm->dataStable = 0;
    }

    if (!rm->scl || !change) {
        return;
    }

    if (value) {
        if (rm->phase == PHASE_WRITE && rm->counter == 0) {
            boardTimerAdd(rm->timer, boardSystemTime() + boardFrequency() * 3 / 1000);
            rm->phase = PHASE_WRITING;
            rm->romData[rm->address] = rm->data;
        }
        else {
            // Stop Data Transfer
            rm->phase = PHASE_IDLE;
        }
    }
    else {
        if (rm->phase == PHASE_READ && rm->counter == 0) {
            rm->address = (rm->address + 1) & rm->romMask;
            rm->data = rm->romData[rm->address];
        }
        // Start Data Transfer
        rm->phase = PHASE_COMMAND;
        rm->counter = 0;
    }
}

int microchip24x00GetSda(Microchip24x00* rm) {
    return rm->sda;
}

int microchip24x00GetScl(Microchip24x00* rm) {
    return rm->scl;
}
