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
#include <time.h>
extern "C" {
#include "MsxBusPi.h"
#include "MsxBus.h"
#include "RomLoader.h"
extern void ledSetSlot1Busy();
extern void ledSetSlot2Busy();
extern void checkInt(void);
};
//#define FAKE_ROM
#ifdef WIN32
class CMSXBUS
{
	public:
		CMSXBUS(int slot) {this.slot = slot;};
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
	
	inline void timelog();

private:
    bool inserted;
	int inst = 0;
    int  slot;
	int size;
//	UInt8 *bin;
	int page[4];
	int skip = 0;
	struct timespec t1, t2;	
};

CMSXBUS::CMSXBUS(int mbSlot) : 
    slot(mbSlot)
{
}

CMSXBUS::~CMSXBUS() {
//	free(bin);
	msxclose();
}

void CMSXBUS::timelog()
{
	double elapsedTime;
	if (!(++inst % 1000))
	{
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t2);
		elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000000000.0;      // sec to ns
		elapsedTime += (t2.tv_nsec - t1.tv_nsec) ;   // us to ns
//		printf("elapsed time: %fns, %fns/i\n", elapsedTime, elapsedTime / inst);	
	}
}


int CMSXBUS::readMemory(UInt16 address)
{
	checkInt();
	int byte = msxread(slot, address);
	int value = byte;
	static int time = 0;
#ifdef FAKE_ROM	
	int p = -1;
	if (size > 32768) {
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
	} else if (size > 0)
	{
		if (size < 32768)
			value = bin[address & 0x3fff];
		else
			value = bin[address & 0xcfff];
	}
	if (slot == 1 && byte != value)
		printf("read%d: 0x%04x-%02x:%02x\n", slot, address, value, byte);
	if (address >= 0x8000)
		value = 0xff;
	byte = value;
#endif
#ifdef RPMC_FRONTLED
	if (time++ > 100)
	{
		if (slot)
			ledSetSlot2Busy();
		else
			ledSetSlot1Busy();
		time = 0;
	}
#endif
    return byte;
}

int CMSXBUS::writeMemory(UInt16 address, UInt8 value)
{
	checkInt();
	msxwrite(slot, address, value);
//	msxwrite(slot, address, value);
	//if (slot == 1)
		//printf("write%d: 0x%04x-%02x\n", slot, address, value);
    return true;
}

int CMSXBUS::readIo(UInt16 port)
{
	checkInt();
	int value = msxreadio(port);
 	//printf("readio(%02x): %02x\n", port, value);
    return value;
}

int CMSXBUS::writeIo(UInt16 port, UInt8 value)
{
	checkInt();
	msxwriteio(port, value);
 	printf("writeio(%02x): %02x\n", port, value);
    return true;
}

#endif

static CMSXBUS* MSXBUSs[2] = { NULL, NULL };

static void InitializeMSXBUSs()
{
    if (MSXBUSs[0] == NULL) {
        MSXBUSs[0] = new CMSXBUS(0);
	printf("MSXBUSs[0]=%d\n", MSXBUSs[0]);
	MSXBUSs[1] = new CMSXBUS(1);
	printf("MSXBUSs[1]=%d\n", MSXBUSs[1]);
    }
    msxinit();
}

static void DeinitializeMSXBUSs()
{
    if (MSXBUSs[0]!= NULL or MSXBUSs[1]!= NULL) {
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

extern "C" MbHandle* msxBusCreate(int cartSlot, int slot)
{
	InitializeMSXBUSs();
	printf("msxBusCreate %d\n", cartSlot, slot);
	return (MbHandle*)MSXBUSs[cartSlot];
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
