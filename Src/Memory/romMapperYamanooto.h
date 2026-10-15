/*****************************************************************************
**
** Yamanooto (SCC Alliance / genami.shop) flash cartridge mapper for blueMSX.
**
** Reference behaviour: openMSX src/memory/Yamanooto.cc
**   - K5 (Konami SCC) mapper with 8-bit bank values and 10-bit bank registers
**     (up to 8 Mbyte of flash), plus an OFFR offset register (32 kbyte units)
**     so a single flash can carry many games.
**   - Optional K4 (Konami) mode selected via the CFGR register.
**   - Register block at 0x7FFC..0x7FFF (ENAR/OFFR/CFGR/FPGA), only readable
**     after ENAR register-access is enabled with REGEN (0x01) bit.
**   - Integrated SCC (Compatible / Plus mode at 0x9800 / 0xB800).
**
** Not yet implemented: onboard PSG on I/O ports 0x10-0x11 (and ECHO mode on
** 0xA0-0xA1), flash write/persist behaviour (WREN), FPGA register protocol.
**
*****************************************************************************/
#ifndef ROMMAPPER_YAMANOOTO_H
#define ROMMAPPER_YAMANOOTO_H

#include "MsxTypes.h"

int romMapperYamanootoCreate(const char* filename, UInt8* romData,
                             int size, int slot, int sslot, int startPage);

#endif