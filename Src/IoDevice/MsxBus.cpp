/*****************************************************************************
** $Source: /cygdrive/d/Private/_SVNROOT/bluemsx/blueMSX/Src/IoDevice/MSXBUS.cpp,v $
**
** $Revision: 1.8 $
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

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
extern "C" {
#include "MsxBusPi.h"
#include "MsxBus.h"
#include "RomLoader.h"
};

#ifdef WIN32
class CMSXBUS
{
	public:
		CMSXBUS() {};
		~CMSXBUS() {};

    inline int readMemory(UInt16 address) { return 0xff };
    inline int writeMemory(UInt16 address, UInt8 value) { return 0xff };
    
    inline int readIo(UInt16 port) { return 0xff };
    inline int writeIo(UInt16 port, UInt8 value) { return 0xff };
};

#else

class CMSXBUS 
{
public:
    CMSXBUS(int mbSlot = -1);
    ~CMSXBUS();

    inline int readMemory(UInt16 address);
    inline int writeMemory(UInt16 address, UInt8 value);
    
    inline int readIo(UInt16 port);
    inline int writeIo(UInt16 port, UInt8 value);

private:
    bool inserted;
    int  slot;
	UInt8 *bin;
	int page[4];
	int skip = 0;
};

CMSXBUS::CMSXBUS(int mbSlot) : 
    slot(mbSlot)
{
	int size, i;
	bin = romLoad("./MMCSD514.ROM", NULL, &size);
	for(i=0; i<4; i++)
		page[i]=i;
}

CMSXBUS::~CMSXBUS() {
	free(bin);
	msxclose();
}

int CMSXBUS::readMemory(UInt16 address)
{
	int value = msxread(slot, address);
#if 0	
	int p = -1;
	if (address >= 0x4000 && address <= 0x5fff)
		p = 0;
	else if (address >= 0x6000 && address <= 0x7fff)
		p = 1;
	else if (address >= 0x8000 && address <= 0x9fff)
		p = 2;
	else if (address >= 0xa000 && address <= 0xbfff)
		p = 3;
	if (p > -1 && skip == 0)
		value = bin[address & 0x1fff + page[p] * 0x2000];
#endif
//	printf("read%d: 0x%04x-%02x\n", slot, address, value);
    return value;
	//return (address >=0x4000 && address < 0xc000 ? bin[address-0x4000] : 0);
}

int CMSXBUS::writeMemory(UInt16 address, UInt8 value)
{
	msxwrite(slot, address, value);
#if 0
	if (address == 0x5000)
		page[0] = value;
	else if (address == 0x7000)
		page[1] = value;
	else if (address == 0x9000)
		page[2] = value;
	else if (address == 0xb000)
		page[3] = value;
	else if (address == 0x4000)
		skip = 1;
#endif	
  	printf("write%d: 0x%04x-%02x\n", slot, address, value);
    return true;
}

int CMSXBUS::readIo(UInt16 port)
{
	int value = msxreadio(port);
 	printf("readio(%02x): %02x\n", port, value);
   return value;
}

int CMSXBUS::writeIo(UInt16 port, UInt8 value)
{
	msxwriteio(port, value);
 	printf("writeio(%02x): %02x\n", port, value);
    return true;
}

#endif

static CMSXBUS* MSXBUSs[2] = { NULL, NULL };

static void InitializeMSXBUSs()
{
    if (MSXBUSs[0] == NULL) {
#ifndef WIN32
		msxinit();
#endif
        MSXBUSs[0] = new CMSXBUS(1);
		printf("MSXBUSs[0]=%d\n", MSXBUSs[0]);
		MSXBUSs[1] = new CMSXBUS(2);
		printf("MSXBUSs[0]=%d\n", MSXBUSs[0]);
    }
}

static void DeinitializeMSXBUSs()
{
    if (MSXBUSs[0]!= NULL) {
#ifndef WIN32		
		msxclose();
#endif		
        delete MSXBUSs[0];
		delete MSXBUSs[1];
    }
}



/////////////////////////////////////////////////////////////
//
// Public C interface

extern "C" MbHandle* msxBusCreate(int slot)
{
	InitializeMSXBUSs();
	printf("msxBusCreate %d\n", slot);
	if (slot == 1 || slot == 2)
		return (MbHandle*)MSXBUSs[slot-1];
	return 0;
}

extern "C" void msxBusDestroy(MbHandle* mpHandle)
{
    DeinitializeMSXBUSs();
}

extern "C" int msxBusRead(MbHandle* mpHandle, UInt16 address)
{
    return ((CMSXBUS*)mpHandle)->readMemory(address);
}

extern "C" int msxBusWrite(MbHandle* mpHandle, UInt16 address, UInt8 value)
{
    return ((CMSXBUS*)mpHandle)->writeMemory(address, value);
}

extern "C" int msxBusReadIo(MbHandle* mpHandle, UInt16 port)
{
    return ((CMSXBUS*)mpHandle)->readIo(port);
}

extern "C" int msxBusWriteIo(MbHandle* mpHandle, UInt16 port, UInt8 value)
{
    return ((CMSXBUS*)mpHandle)->writeIo(port, value);
}

extern "C" int msxBusSupported()
{
#ifdef WII
    return 0;
#else
    return 1;
#endif
}