/*****************************************************************************
** $Source: /cygdrive/d/Private/_SVNROOT/bluemsx/blueMSX/Src/Memory/romMapperGameReader.c,v $
**
** $Revision: 1.8 $
**
** $Date: 2008-03-30 18:38:44 $
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
#include "romMapperMsxBus.h"
#include "MediaDb.h"
#include "SlotManager.h"
#include "DeviceManager.h"
#include "SaveState.h"
#include "IoPort.h"
#include "MsxBus.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static FILE* f = NULL;

typedef struct {
    int deviceHandle;
    MbHandle* msxBus;
    int slot;
    int sslot;
    int cartSlot;
} RomMapperMsxBus;

static void saveState(RomMapperMsxBus* rm)
{
    SaveState* state = saveStateOpenForWrite("mapperMsxBus");
    saveStateClose(state);
}

static void loadState(RomMapperMsxBus* rm)
{
    SaveState* state = saveStateOpenForRead("mapperMsxBus");
    saveStateClose(state);
}


static void destroy(RomMapperMsxBus* rm)
{
    if (rm->msxBus != NULL) {
        msxBusDestroy(rm->msxBus);
        ioPortUnregisterUnused(rm->cartSlot);
        slotUnregister(rm->slot, rm->sslot, 0);
    }
    deviceManagerUnregister(rm->deviceHandle);

//    fclose(f);
    f = NULL;

    free(rm);
}

static UInt8 readIo(RomMapperMsxBus* rm, UInt16 port)
{
    return msxBusReadIo(rm->msxBus, port);
}

static void writeIo(RomMapperMsxBus* rm, UInt16 port, UInt8 value)
{
    msxBusWriteIo(rm->msxBus, port, value);
}

static UInt8 read(RomMapperMsxBus* rm, UInt16 address) 
{
    return msxBusRead(rm->msxBus, address);
}

static void write(RomMapperMsxBus* rm, UInt16 address, UInt8 value) 
{
	msxBusWrite(rm->msxBus, address, value);
}

int romMapperMsxBusCreate(int cartSlot, int slot, int sslot) 
{
    DeviceCallbacks callbacks = { destroy, NULL, saveState, loadState };
    RomMapperMsxBus* rm;
    int i;
	
    rm = malloc(sizeof(RomMapperMsxBus));

    rm->deviceHandle = deviceManagerRegister(ROM_MSXBUS, &callbacks, rm);

    rm->slot     = slot;
    rm->sslot    = sslot;
    rm->cartSlot = cartSlot;

    rm->msxBus = msxBusCreate(cartSlot+1);
//	printf("MSXBus created. msxBus=%d slot=%d sslot=%d\n", rm->msxBus, slot, sslot);

    if (rm->msxBus != NULL) {
        ioPortRegisterUnused(cartSlot, readIo, writeIo, rm);
        slotRegister(slot, sslot, 0, 8, read, read, write, destroy, rm);
        for (i = 0; i < 8; i++) {   
            slotMapPage(rm->slot, rm->sslot, i, NULL, 0, 0);
        }
		printf("MSXBus created. slot=%d sslot=%d\n", slot, sslot);
    }

    return 1;
}

