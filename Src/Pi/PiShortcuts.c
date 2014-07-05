/*****************************************************************************
**
** blueberryMSX
** https://github.com/pokebyte/blueberryMSX
**
** An MSX Emulator for Raspberry Pi based on blueMSX
**
** Copyright (C) 2003-2006 Daniel Vik
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

#include "PiShortcuts.h"
#include <SDL.h>
#include "IniFileParser.h"
#include "StrcmpNoCase.h"
#include "Actions.h"

static char shortcutsDir[512];

extern Properties *properties;

typedef struct {
    unsigned type : 4;
    unsigned mods : 12;
    unsigned key  : 16;
} ShortcutHotkey;

static const ShortcutHotkey quitHotKey = {
    HOTKEY_TYPE_KEYBOARD, 0, SDLK_F12,
};
static const ShortcutHotkey disableFrameskipHotKey = {
    HOTKEY_TYPE_KEYBOARD, 0, SDLK_F11,
};
static const ShortcutHotkey lowFrameskipHotKey = {
    HOTKEY_TYPE_KEYBOARD, 0, SDLK_F10,
};
static const ShortcutHotkey mediumFrameskipHotKey = {
    HOTKEY_TYPE_KEYBOARD, 0, SDLK_F9,
};
static const ShortcutHotkey highFrameskipHotKey = {
    HOTKEY_TYPE_KEYBOARD, 0, SDLK_F8,
};

struct Shortcuts {
    ShortcutHotkey quit;
    ShortcutHotkey fdcTiming;
    ShortcutHotkey spritesEnable;
    ShortcutHotkey switchMsxAudio;
    ShortcutHotkey switchFront;
    ShortcutHotkey switchPause;
    ShortcutHotkey captureAudio;
    ShortcutHotkey captureScreenshot;
    ShortcutHotkey cpuStateQuickLoad;
    ShortcutHotkey cpuStateQuickSave;

    ShortcutHotkey cartRemove[2];
    ShortcutHotkey cartAutoReset;

    ShortcutHotkey diskQuickChange;
    ShortcutHotkey diskRemove[2];
    ShortcutHotkey diskAutoReset;

    ShortcutHotkey casRewind;
    ShortcutHotkey casRemove;
    ShortcutHotkey casToggleReadonly;
    ShortcutHotkey casAutoRewind;
    ShortcutHotkey casSave;

    ShortcutHotkey emulationRunPause;
    ShortcutHotkey emulationStop;
    ShortcutHotkey emuSpeedFull;
    ShortcutHotkey emuSpeedNormal;
    ShortcutHotkey emuSpeedInc;
    ShortcutHotkey emuSpeedDec;
    ShortcutHotkey emuSpeedToggle;
    ShortcutHotkey resetSoft;
    ShortcutHotkey resetHard;
    ShortcutHotkey resetClean;
    ShortcutHotkey volumeIncrease;
    ShortcutHotkey volumeDecrease;
    ShortcutHotkey volumeMute;
    ShortcutHotkey volumeStereo;
    ShortcutHotkey windowSizeNormal;
    ShortcutHotkey windowSizeFullscreen;
    ShortcutHotkey windowSizeFullscreenToggle;

    struct {
        int maxSpeedIsSet;
    } state;
};


#define LOAD_SHORTCUT(hotkey) loadShortcut(iniFile, #hotkey, &shortcuts->hotkey)

#define HOTKEY_EQ(hotkey1, hotkey2) (*(UInt32*)&hotkey1 == *(UInt32*)&hotkey2)


static int stringToHotkey(const char* name)
{
    int i;

    for (i = 0; i < SDLK_LAST; i++) {
        char* sdlName = SDL_GetKeyName(i);
        if (0 == strcmpnocase(name, sdlName)) {
            return i;
        }
    }
    return -1;
}

static int stringToMod(const char* name)
{
    if (strcmpnocase(name, "left shift")        == 0) return 1 << 0;
    if (strcmpnocase(name, "right shift")       == 0) return 1 << 1;
    if (strcmpnocase(name, "left ctrl")         == 0) return 1 << 2;
    if (strcmpnocase(name, "right ctrl")        == 0) return 1 << 3;
    if (strcmpnocase(name, "left alt")          == 0) return 1 << 4;
    if (strcmpnocase(name, "right alt")         == 0) return 1 << 5;
    if (strcmpnocase(name, "right windows key") == 0) return 1 << 6;
    if (strcmpnocase(name, "left windows key")  == 0) return 1 << 7;

    return 0;
}

static void loadShortcut(IniFile *iniFile, char* name, ShortcutHotkey* hotkey)
{
    char buffer[512];
    char* token;
    int key;
    
    hotkey->type = HOTKEY_TYPE_NONE;
    hotkey->mods = 0;
    hotkey->key  = 0;

    if (!iniFileGetString(iniFile, "Shortcuts", name, "", buffer, sizeof(buffer))) {
        return;
    }

    token = strtok(buffer, "|");
    if (token == NULL) {
        return;
    }
    key = stringToHotkey(token);
    if (key >= 0) {
        hotkey->key  = key;
        hotkey->type = HOTKEY_TYPE_KEYBOARD;
    }
    
    while (token = strtok(NULL, "|")) {
        hotkey->mods |= stringToMod(token);
    }
}

void shortcutsSetDirectory(char* directory)
{
    strcpy(shortcutsDir, directory);
}

Shortcuts* shortcutsCreate()
{
    char filename[512];
    Shortcuts* shortcuts = (Shortcuts*)calloc(1, sizeof(Shortcuts));

    sprintf(filename, "%s/blueMSX.shortcuts", shortcutsDir);

    IniFile *iniFile = iniFileOpen(filename);

    LOAD_SHORTCUT(switchMsxAudio);
    LOAD_SHORTCUT(spritesEnable);
    LOAD_SHORTCUT(fdcTiming);
    LOAD_SHORTCUT(switchFront);
    LOAD_SHORTCUT(switchPause);
    LOAD_SHORTCUT(quit);
    LOAD_SHORTCUT(captureAudio);
    LOAD_SHORTCUT(captureScreenshot);
    LOAD_SHORTCUT(cpuStateQuickLoad);
    LOAD_SHORTCUT(cpuStateQuickSave);
    
    LOAD_SHORTCUT(cartRemove[0]);
    LOAD_SHORTCUT(cartRemove[1]);
    LOAD_SHORTCUT(cartAutoReset);
    
    LOAD_SHORTCUT(diskRemove[0]);
    LOAD_SHORTCUT(diskRemove[1]);
    LOAD_SHORTCUT(diskQuickChange);
    LOAD_SHORTCUT(diskAutoReset);

    LOAD_SHORTCUT(casRewind);
    LOAD_SHORTCUT(casRemove);
    LOAD_SHORTCUT(casToggleReadonly);
    LOAD_SHORTCUT(casAutoRewind);
    LOAD_SHORTCUT(casSave);
    
    LOAD_SHORTCUT(emulationRunPause);
    LOAD_SHORTCUT(emulationStop);
    LOAD_SHORTCUT(emuSpeedFull);
    LOAD_SHORTCUT(emuSpeedToggle);
    LOAD_SHORTCUT(emuSpeedNormal);
    LOAD_SHORTCUT(emuSpeedInc);
    LOAD_SHORTCUT(emuSpeedDec);
    LOAD_SHORTCUT(windowSizeNormal);
    LOAD_SHORTCUT(windowSizeFullscreen);
    LOAD_SHORTCUT(windowSizeFullscreenToggle);
    LOAD_SHORTCUT(resetSoft);
    LOAD_SHORTCUT(resetHard);
    LOAD_SHORTCUT(resetClean);
    LOAD_SHORTCUT(volumeIncrease);
    LOAD_SHORTCUT(volumeDecrease);
    LOAD_SHORTCUT(volumeMute);
    LOAD_SHORTCUT(volumeStereo);

    iniFileClose(iniFile);

    return shortcuts;
}

void shortcutsDestroy(Shortcuts* shortcuts)
{
    free(shortcuts);
}

void shortcutCheckDown(Shortcuts* s, int type, int mods, int keySym)
{
    ShortcutHotkey key = { type, mods, keySym };

    if (HOTKEY_EQ(key, s->emuSpeedFull)) {
        if (s->state.maxSpeedIsSet == 0) {
            actionMaxSpeedSet();
            s->state.maxSpeedIsSet = 1;
        }
    }
}

void shortcutCheckUp(Shortcuts* s, int type, int mods, int keySym)
{
    ShortcutHotkey key = { type, mods, keySym };

    if (s->state.maxSpeedIsSet) {
        actionMaxSpeedRelease();
        s->state.maxSpeedIsSet = 0;
    }

    if (HOTKEY_EQ(key, quitHotKey)) actionQuit();

    if (HOTKEY_EQ(key, disableFrameskipHotKey)) {
        properties->video.frameSkip = 0;
    }
    if (HOTKEY_EQ(key, lowFrameskipHotKey)) {
        properties->video.frameSkip = 1;
    }
    if (HOTKEY_EQ(key, mediumFrameskipHotKey)) {
        properties->video.frameSkip = 2;
    }
    if (HOTKEY_EQ(key, highFrameskipHotKey)) {
        properties->video.frameSkip = 3;
    }

    if (HOTKEY_EQ(key, s->quit))                         actionQuit();
    if (HOTKEY_EQ(key, s->fdcTiming))                    actionToggleFdcTiming();
    if (HOTKEY_EQ(key, s->spritesEnable))                actionToggleSpriteEnable();
    if (HOTKEY_EQ(key, s->switchMsxAudio))               actionToggleMsxAudioSwitch();
    if (HOTKEY_EQ(key, s->switchFront))                  actionToggleFrontSwitch();
    if (HOTKEY_EQ(key, s->switchPause))                  actionTogglePauseSwitch();
    if (HOTKEY_EQ(key, s->captureAudio))                 actionToggleWaveCapture();
    if (HOTKEY_EQ(key, s->captureScreenshot))            actionScreenCapture();
    if (HOTKEY_EQ(key, s->cpuStateQuickLoad))            actionQuickLoadState();
    if (HOTKEY_EQ(key, s->cpuStateQuickSave))            actionQuickSaveState();

    if (HOTKEY_EQ(key, s->cartRemove[0]))                actionCartRemove1();
    if (HOTKEY_EQ(key, s->cartRemove[1]))                actionCartRemove2();
    if (HOTKEY_EQ(key, s->cartAutoReset))                actionToggleCartAutoReset();

    if (HOTKEY_EQ(key, s->diskQuickChange))              actionDiskQuickChange();
    if (HOTKEY_EQ(key, s->diskRemove[0]))                actionDiskRemoveA();
    if (HOTKEY_EQ(key, s->diskRemove[1]))                actionDiskRemoveB();
    if (HOTKEY_EQ(key, s->diskAutoReset))                actionToggleDiskAutoReset();

    if (HOTKEY_EQ(key, s->casRewind))                    actionCasRewind();
    if (HOTKEY_EQ(key, s->casRemove))                    actionCasRemove();
    if (HOTKEY_EQ(key, s->casToggleReadonly))            actionCasToggleReadonly();
    if (HOTKEY_EQ(key, s->casAutoRewind))                actionToggleCasAutoRewind();
    if (HOTKEY_EQ(key, s->casSave))                      actionCasSave();

    if (HOTKEY_EQ(key, s->emulationRunPause))            actionEmuTogglePause();
    if (HOTKEY_EQ(key, s->emulationStop))                actionEmuStop();
    if (HOTKEY_EQ(key, s->emuSpeedNormal))               actionEmuSpeedNormal();
    if (HOTKEY_EQ(key, s->emuSpeedInc))                  actionEmuSpeedIncrease();
    if (HOTKEY_EQ(key, s->emuSpeedDec))                  actionEmuSpeedDecrease();
    if (HOTKEY_EQ(key, s->emuSpeedToggle))               actionMaxSpeedToggle();
    if (HOTKEY_EQ(key, s->resetSoft))                    actionEmuResetSoft();
    if (HOTKEY_EQ(key, s->resetHard))                    actionEmuResetHard();
    if (HOTKEY_EQ(key, s->resetClean))                   actionEmuResetClean();
    if (HOTKEY_EQ(key, s->volumeIncrease))               actionVolumeIncrease();
    if (HOTKEY_EQ(key, s->volumeDecrease))               actionVolumeDecrease();
    if (HOTKEY_EQ(key, s->volumeMute))                   actionMuteToggleMaster();
    if (HOTKEY_EQ(key, s->volumeStereo))                 actionVolumeToggleStereo();
    if (HOTKEY_EQ(key, s->windowSizeNormal))             actionWindowSizeNormal();
    if (HOTKEY_EQ(key, s->windowSizeFullscreen))         actionWindowSizeFullscreen();
    if (HOTKEY_EQ(key, s->windowSizeFullscreenToggle))   actionFullscreenToggle();
}


