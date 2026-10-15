#include "romMapperYamanooto.h"
#include "MediaDb.h"
#include "SlotManager.h"
#include "DeviceManager.h"
#include "SCC.h"
#include "Board.h"
#include "SaveState.h"
#include "Log.h"

#include <stdlib.h>
#include <string.h>

#define YN_ENAR   0x7fff
#define YN_OFFR   0x7ffe
#define YN_CFGR   0x7ffd
#define YN_FPGA   0x7ffc

#define YN_REGEN   0x01
#define YN_WREN    0x10
#define YN_MDIS    0x01
#define YN_ECHO    0x02
#define YN_ROMDIS  0x04
#define YN_K4      0x08
#define YN_SUBOFF  0x30
#define YN_FPGA_EN 0x40
#define YN_BUSY    0x80

static const UInt8 ynFpgaId[5] = { 0xff, 0x1f, 0x23, 0x00, 0x00 };

typedef struct {
    int deviceHandle;
    UInt8* romData;
    int slot;
    int sslot;
    int startPage;
    int romMask;
    int enableReg;
    int offsetReg;
    int configReg;
    int bankRegs[4];
    int rawBanks[4];
    int sccMode;
    int fpgaFsm;
    SCC* scc;
} RomMapperYamanooto;

static void ynWriteConfigReg(RomMapperYamanooto* rm, UInt8 value)
{
    (void)rm;
    (void)value;
    /* ECHO toggles onboard PSG ports; PSG not implemented yet. */
}

static UInt16 ynMirror(UInt16 address)
{
    if (address < 0x4000 || address >= 0xc000) {
        address ^= 0x8000;
    }
    return address;
}

static int ynIsSccAccess(RomMapperYamanooto* rm, UInt16 address)
{
    if (rm->configReg & YN_K4) {
        return 0;
    }
    if (rm->sccMode & 0x20) {
        return (rm->rawBanks[3] & 0x80) && address >= 0xb800 && address < 0xbffe;
    }
    return ((rm->rawBanks[2] & 0x3f) == 0x3f) && address >= 0x9800 && address < 0xa000;
}

static UInt32 ynFlashAddr(RomMapperYamanooto* rm, UInt16 address)
{
    int page8kB = (address >> 13) - 2;
    int bank = (rm->bankRegs[page8kB] & 0x3ff) & rm->romMask;
    return (UInt32)(bank << 13) | (address & 0x1fff);
}

static void saveState(RomMapperYamanooto* rm)
{
    SaveState* state = saveStateOpenForWrite("mapperYamanooto");
    char tag[16];
    int i;

    for (i = 0; i < 4; i++) {
        sprintf(tag, "rawBanks%d", i);
        saveStateSet(state, tag, rm->rawBanks[i]);
    }

    saveStateSet(state, "enableReg",  rm->enableReg);
    saveStateSet(state, "offsetReg",  rm->offsetReg);
    saveStateSet(state, "configReg",  rm->configReg);
    saveStateSet(state, "sccMode",    rm->sccMode);
    saveStateSet(state, "fpgaFsm",    rm->fpgaFsm);

    saveStateClose(state);

    sccSaveState(rm->scc);
}

static void loadState(RomMapperYamanooto* rm)
{
    SaveState* state = saveStateOpenForRead("mapperYamanooto");
    char tag[16];
    int i;

    for (i = 0; i < 4; i++) {
        sprintf(tag, "rawBanks%d", i);
        rm->rawBanks[i] = saveStateGet(state, tag, i);
    }

    rm->enableReg = saveStateGet(state, "enableReg", 0);
    rm->offsetReg = saveStateGet(state, "offsetReg", 0);
    rm->configReg = saveStateGet(state, "configReg", 0);
    rm->sccMode   = saveStateGet(state, "sccMode", 0);
    rm->fpgaFsm   = saveStateGet(state, "fpgaFsm", 0);

    saveStateClose(state);

    sccLoadState(rm->scc);

    for (i = 0; i < 4; i++) {
        rm->bankRegs[i] = rm->rawBanks[i] + ((rm->offsetReg << 2) | ((rm->configReg & YN_SUBOFF) >> 4));
    }
    sccSetMode(rm->scc, (rm->sccMode & 0x20) ? SCC_PLUS : SCC_REAL);
}

static void destroy(RomMapperYamanooto* rm)
{
    slotUnregister(rm->slot, rm->sslot, rm->startPage);
    deviceManagerUnregister(rm->deviceHandle);
    sccDestroy(rm->scc);

    free(rm->romData);
    free(rm);
}

static void reset(RomMapperYamanooto* rm)
{
    int i;

    rm->enableReg  = 0;
    rm->offsetReg  = 0;
    rm->configReg  = 0;
    rm->sccMode    = 0;
    rm->fpgaFsm    = 0;

    for (i = 0; i < 4; i++) {
        rm->rawBanks[i]  = i;
        rm->bankRegs[i]  = i;
    }

    sccSetMode(rm->scc, SCC_REAL);
    sccReset(rm->scc);
}

static UInt8 regRead(RomMapperYamanooto* rm, UInt16 address)
{
    switch (address) {
    case YN_FPGA:
        if (!(rm->configReg & YN_FPGA_EN)) {
            return 0xff;
        }
        return (rm->fpgaFsm < 5) ? ynFpgaId[rm->fpgaFsm] : 0xff;
    case YN_CFGR:
        return (UInt8)(rm->configReg | YN_BUSY);
    case YN_OFFR:
        return (UInt8)rm->offsetReg;
    case YN_ENAR:
        return (UInt8)rm->enableReg;
    }
    return 0xff;
}

static UInt8 read(RomMapperYamanooto* rm, UInt16 address)
{
    address += 0x4000;

    if (address >= YN_FPGA && address <= YN_ENAR && (rm->enableReg & YN_REGEN)) {
        return regRead(rm, address);
    }

    address = ynMirror(address);

    if (ynIsSccAccess(rm, address)) {
        return sccRead(rm->scc, (UInt8)(address & 0xff));
    }

    if (rm->configReg & YN_ROMDIS) {
        return 0xff;
    }

    return rm->romData[ynFlashAddr(rm, address)];
}

static UInt8 peek(RomMapperYamanooto* rm, UInt16 address)
{
    address += 0x4000;

    if (address >= YN_FPGA && address <= YN_ENAR && (rm->enableReg & YN_REGEN)) {
        return regRead(rm, address);
    }

    address = ynMirror(address);

    if (ynIsSccAccess(rm, address)) {
        return sccPeek(rm->scc, (UInt8)(address & 0xff));
    }

    if (rm->configReg & YN_ROMDIS) {
        return 0xff;
    }

    return rm->romData[ynFlashAddr(rm, address)];
}

static void setBank(RomMapperYamanooto* rm, int page8kB, UInt8 value)
{
    int offset = (rm->offsetReg << 2) | ((rm->configReg & YN_SUBOFF) >> 4);

    rm->rawBanks[page8kB]  = value;
    rm->bankRegs[page8kB]  = (value + offset) & 0x3ff;
}

static void write(RomMapperYamanooto* rm, UInt16 address, UInt8 value)
{
    int page8kB;

    address += 0x4000;

    if (address >= YN_FPGA && address <= YN_ENAR) {
        if (address == YN_ENAR) {
            rm->enableReg = value;
        } else if (rm->enableReg & YN_REGEN) {
            switch (address) {
            case YN_FPGA:
                if (!(rm->configReg & YN_FPGA_EN)) {
                    break;
                }
                if (rm->fpgaFsm < 4) {
                    rm->fpgaFsm++;
                } else {
                    rm->fpgaFsm = 0;
                }
                break;
            case YN_CFGR:
                rm->configReg      = value;
                ynWriteConfigReg   (rm, value);
                break;
            case YN_OFFR:
                rm->offsetReg = value;
                break;
            }
        }
        return;
    }

    address = ynMirror(address);

    if (rm->enableReg & YN_WREN) {
        /* Flash write mode: not persisted yet, ignore. */
        return;
    }

    page8kB = (address >> 13) - 2;

    if (rm->configReg & YN_K4) {
        if ((rm->configReg & YN_MDIS) == 0 && address >= 0x6000) {
            setBank(rm, page8kB, value);
        }
        return;
    }

    if (ynIsSccAccess(rm, address)) {
        sccWrite(rm->scc, (UInt8)(address & 0xff), value);
    }

    if ((address & 0x1800) == 0x1000 && (rm->configReg & YN_MDIS) == 0) {
        setBank(rm, page8kB, value);
    }

    if ((address & 0xfffe) == 0xbffe) {
        rm->sccMode = value;
        sccSetMode(rm->scc, (value & 0x20) ? SCC_PLUS : SCC_REAL);
    }
}

int romMapperYamanootoCreate(const char* filename, UInt8* romData,
                             int size, int slot, int sslot, int startPage)
{
    DeviceCallbacks callbacks = { destroy, reset, saveState, loadState };
    RomMapperYamanooto* rm;
    int i;
    int origSize = size;

    (void)filename;

    size = 0x8000;
    while (size < origSize) {
        size *= 2;
    }

    rm = malloc(sizeof(RomMapperYamanooto));

    rm->deviceHandle = deviceManagerRegister(ROM_YAMANOOTO, &callbacks, rm);
    slotRegister(slot, sslot, startPage, 4, read, peek, write, destroy, rm);

    rm->romData = calloc(1, size);
    memcpy(rm->romData, romData, origSize);
    rm->romMask = size / 0x2000 - 1;
    rm->slot  = slot;
    rm->sslot = sslot;
    rm->startPage  = startPage;

    rm->scc = sccCreate(boardGetMixer());
    sccSetMode(rm->scc, SCC_REAL);

    reset(rm);

    LOG_INFO("Yamanooto mapper: %d-byte image at slot %d-%d page %d", origSize, slot, sslot, startPage);

    for (i = 0; i < 4; i++) {
        slotMapPage(rm->slot, rm->sslot, rm->startPage + i, rm->romData, 0, 0);
    }

    return 1;
}