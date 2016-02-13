/*****************************************************************************
** $Source: /cygdrive/d/Private/_SVNROOT/bluemsx/blueMSX/Src/IoDevice/MSXSlotPi.cpp,v $
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
extern "C" {
#include "MSXSlotPi.h"
};

#ifdef _WIN32
#include "msxgr.h"
#else
class CMSXSlotPi
{
	public:
		CMSXSlotPi() {};
		~CMSXSlotPi() {};

        int  Init() { return 0; }

        bool IsSlotEnable(int) { return false; }
		bool IsCartridgeInserted(int) { return false; }

		int  ReadMemory(int,char*,int,int) { return 0; }
		int  WriteMemory(int,char*,int,int) { return 0; }
		int  WriteIO(int,char*,int,int) { return 0; }
		int  ReadIO(int,char*,int,int) { return 0; }
};
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#include <Windows.h>
#else
#define GlobalAlloc(xxx, addr) malloc(addr)
#define GlobalFree(addr) free(addr)
#endif


//#define ENABLE_DEBUG
#ifdef ENABLE_DEBUG
#define dprintf printf
#else
#define dprintf if (0) printf
#endif


static CMSXSlotPi* MsxPi;

struct MSXSlotPi 
{
public:
    MSXSlotPi(int grSlot = -1);
    ~MSXSlotPi();

    inline bool readMemory(UInt16 address, void* buffer, int length);
    inline bool writeMemory(UInt16 address, void* buffer, int length);
    
    inline bool readIo(UInt16 port, UInt8* value);
    inline bool writeIo(UInt16 port, UInt8 value);

private:
    bool inserted;
    int  slot;
    char* globalBuffer;
};


MSXSlotPi::MSXSlotPi(int grSlot) : 
    inserted(false),
    slot(grSlot),
    globalBuffer((char*)GlobalAlloc(GPTR, 0x4000))
{
}

MSXSlotPi::~MSXSlotPi() {
    GlobalFree(globalBuffer);
}

bool MSXSlotPi::readMemory(UInt16 address, void* buffer, int length)
{
    if (slot == -1) {
        return false;
    }
    
    if (!inserted) {
        inserted = MsxPi->IsCartridgeInserted(slot);
    }
    
    if (inserted) {
        //printf("### Reading address %.4x - %.4x\n", address, address + length - 1);
        if (MsxPi->ReadMemory(slot, globalBuffer, address, length) != 0) {
            inserted = MsxPi->IsCartridgeInserted(slot);
            return false;
        }
        memcpy(buffer, globalBuffer, length);
    }
    return true;
}

bool MSXSlotPi::writeMemory(UInt16 address, void* buffer, int length)
{
    if (slot == -1) {
        return false;
    }
    
    if (!inserted) {
        inserted = MsxPi->IsCartridgeInserted(slot);
    }
    
    if (inserted) {
        memcpy(globalBuffer, buffer, length);
        //printf("### Writing address %.4x - %.4x\n", address, address + length - 1);
        if (MsxPi->WriteMemory(slot, globalBuffer, address, length) != 0) {
            inserted = MsxPi->IsCartridgeInserted(slot);
            return false;
        }
    }
    return true;
}

bool MSXSlotPi::readIo(UInt16 port, UInt8* value)
{
    if (slot == -1) {
        return false;
    }
    
    if (!inserted) {
        inserted = MsxPi->IsCartridgeInserted(slot);
    }

    if (inserted) {
        if (MsxPi->ReadIO(slot, globalBuffer, port, 1) != 0) {
            inserted = MsxPi->IsCartridgeInserted(slot);
            return false;
        }
        *value = *(UInt8*)globalBuffer;
    }
    return true;
}

bool MSXSlotPi::writeIo(UInt16 port, UInt8 value)
{
    if (slot == -1) {
        return false;
    }
    
    if (!inserted) {
        inserted = MsxPi->IsCartridgeInserted(slot);
    }

    if (inserted) {
        *(UInt8*)globalBuffer = value;
        if (MsxPi->WriteIO(slot, globalBuffer, port, 1) != 0) {
            inserted = MsxPi->IsCartridgeInserted(slot);
            return false;
        }
    }
    return true;
}

#define MAX_GAMEREADERS 2

static MSXSlotPi* MSXSlotPis[MAX_GAMEREADERS] = { NULL, NULL };

static void InitializeMSXSlotPis()
{
    if (MsxPi == NULL) {
        MsxPi = new CMSXSlotPi;

        int msxSlotPiCount = 0;

        if (MsxPi->Init() == 0) {
            for (int i = 0; i < 16 && msxSlotPiCount < MAX_GAMEREADERS; i++) {
                if (MsxPi->IsSlotEnable(i)) {
                    MSXSlotPis[msxSlotPiCount++] = new MSXSlotPi(i);
                }
            }
        }

        for (; msxSlotPiCount < 2; msxSlotPiCount++) {
            MSXSlotPis[msxSlotPiCount] = new MSXSlotPi;
        }
    }
}

static void DeinitializeMSXSlotPis()
{
    if (MsxPi != NULL) {
        for (int i = 0; i < MAX_GAMEREADERS; i++) {
            if (MSXSlotPis[i] != NULL) {
                delete MSXSlotPis[i];
                MSXSlotPis[i] = NULL;
            }
        }
        delete MsxPi;
        MsxPi = NULL;
    }
}

/////////////////////////////////////////////////////////////
//
// Public C interface

extern "C" MpHandle* msxSlotPiCreate(int slot)
{
    InitializeMSXSlotPis();

    return (MpHandle*)MSXSlotPis[slot];
}

extern "C" void msxSlotPiDestroy(MpHandle* mpHandle)
{
    DeinitializeMSXSlotPis();
}

extern "C" int msxSlotPiRead(MpHandle* mpHandle, UInt16 address, void* buffer, int length)
{
    return ((MSXSlotPi*)mpHandle)->readMemory(address, buffer, length) ? 1 : 0;
}

extern "C" int msxSlotPiWrite(MpHandle* mpHandle, UInt16 address, void* buffer, int length)
{
    return ((MSXSlotPi*)mpHandle)->writeMemory(address, buffer, length) ? 1 : 0;
}

extern "C" int msxSlotPiReadIo(MpHandle* mpHandle, UInt16 port, UInt8* value)
{
    return ((MSXSlotPi*)mpHandle)->readIo(port, value) ? 1 : 0;
}

extern "C" int msxSlotPiWriteIo(MpHandle* mpHandle, UInt16 port, UInt8 value)
{
    return ((MSXSlotPi*)mpHandle)->writeIo(port, value) ? 1 : 0;
}

extern "C" int msxSlotPiSupported()
{
#ifdef WII
    return 0;
#else
    return 1;
#endif
}
