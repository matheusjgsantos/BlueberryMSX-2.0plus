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
#include <SDL_keysym.h>
#include "IniFileParser.h"
#include "StrcmpNoCase.h"
#include "Actions.h"

static int sdlkeys[256] = {
    0,
    0, //"LBUTTON", 
    0, //"RBUTTON",
    0,
    0, //"MBUTTON",
    0, //"XBUTTON1",
    0, //"XBUTTON2",
    0,
    SDLK_BACKSPACE ,
    SDLK_TAB,
    0,
    0,
    SDLK_CLEAR,
    SDLK_RETURN,
    0,
    0,
    0, // SHIFT
    0, // CTRL
    0, // ALT
    SDLK_PAUSE ,
    SDLK_CAPSLOCK ,
    0,//"Kana",
    0,
    0,//"Junja",
    0,//"Final",
    0,//"Kanji",
    0,
    SDLK_ESCAPE,
    0,//"Conv",
    0,//"NoConv",
    0,//"Accept",
    SDLK_MODE,
    SDLK_SPACE ,
    SDLK_PAGEUP,
    SDLK_PAGEDOWN ,
    SDLK_END,
    SDLK_HOME,
    SDLK_LEFT,
    SDLK_UP,
    SDLK_RIGHT,
    SDLK_DOWN,
    0,//SDLK_SELECT,
    SDLK_PRINT,
    0,//SDLK_EXECUTE,
    SDLK_PRINT,
    SDLK_INSERT,
    SDLK_DELETE,
    SDLK_HELP,
    '0',
    '1',
    '2',
    '3',
    '4',
    '5',
    '6',
    '7',
    '8',
    '9',
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    'A',
    'B',
    'C',
    'D',
    'E',
    'F',
    'G',
    'H',
    'I',
    'J',
    'K',
    'L',
    'M',
    'N',
    'O',
    'P',
    'Q',
    'R',
    'S',
    'T',
    'U',
    'V',
    'W',
    'X',
    'Y',
    'Z',
    SDLK_LMETA ,
    SDLK_RMETA ,
    0,//SDLK_APPLICATION ,
    0,
    0,//SDLK_SLEEP,
    SDLK_KP0,
    SDLK_KP1,
    SDLK_KP2,
    SDLK_KP3,
    SDLK_KP4,
    SDLK_KP5,
    SDLK_KP6,
    SDLK_KP7,
    SDLK_KP8,
    SDLK_KP9,
    SDLK_KP_MULTIPLY ,
    SDLK_KP_PLUS,
    0,
    SDLK_KP_MINUS ,
    SDLK_KP_PERIOD,
    SDLK_KP_DIVIDE,
    SDLK_F1 ,
    SDLK_F2,
    SDLK_F3,
    SDLK_F4,
    SDLK_F5,
    SDLK_F6,
    SDLK_F7,
    SDLK_F8,
    SDLK_F9,
    SDLK_F10,
    SDLK_F11,
    SDLK_F12,
    SDLK_F13,
    SDLK_F14,
    SDLK_F15,
    0,//SDLK_F16,
    0,//SDLK_F17,
    0,//SDLK_F18,
    0,//SDLK_F19,
    0,//SDLK_F20,
    0,//SDLK_F21,
    0,//SDLK_F22,
    0,//SDLK_F23,
    0,//SDLK_F24,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,//SDLK_NUMLOCKCLEAR ,
    0,//SDLK_SCROLLLOCK ,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    SDLK_LSHIFT ,
    SDLK_RSHIFT ,
    SDLK_LCTRL,
    SDLK_RCTRL,
    SDLK_LALT,
    SDLK_RALT,
    0,//SDLK_AC_BACK ,
    0,//SDLK_AC_FORWARD,
    0,//SDLK_AC_REFRESH,
    0,//SDLK_AC_STOP ,
    0,//SDLK_AC_SEARCH,
    0,//SDLK_AC_BOOKMARKS,
    0,//SDLK_AC_HOME,
    0,//SDLK_MUTE,
    0,//SDLK_VOLUMEDOWN ,
    0,//SDLK_VOLUMEUP,
    0,//SDLK_AUDIONEXT ,
    0,//SDLK_AUDIOPREV ,
    0,//SDLK_AUDIOSTOP ,
    0,//SDLK_AUDIOPLAY ,
    0,//SDLK_MAIL ,
    0,//SDLK_MEDIASELECT ,
    0,//SDLK_APPLICATION ,
    0,//SDLK_APPLICATION ,
    0,
    0,
    ';',
    '+',
    ',',
    '-',
    '.',
    '?',
    '~',
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    '[',
    '\\',
    ']',
    '\"',
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    SDLK_CLEAR ,
    0
};

static char virtualKeys[256][32] = {
    "",
    "", //"LBUTTON", 
    "", //"RBUTTON",
    "Cancel",
    "", //"MBUTTON",
    "", //"XBUTTON1",
    "", //"XBUTTON2",
    "",
    "Backspace",
    "Tab",
    "",
    "",
    "Clear",
    "Enter",
    "",
    "",
    "", // SHIFT
    "", // CTRL
    "", // ALT
    "Pause",
    "CapsLk",
    "Kana",
    "",
    "Junja",
    "Final",
    "Kanji",
    "",
    "esc",
    "Conv",
    "NoConv",
    "Accept",
    "ModeCh",
    "Space",
    "PgUp",
    "PgDown",
    "End",
    "Home",
    "Left",
    "Up",
    "Right",
    "Down",
    "Select",
    "Print",
    "Exec",
    "PrScr",
    "Ins",
    "Del",
    "Help",
    "0",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "A",
    "B",
    "C",
    "D",
    "E",
    "F",
    "G",
    "H",
    "I",
    "J",
    "K",
    "L",
    "M",
    "N",
    "O",
    "P",
    "Q",
    "R",
    "S",
    "T",
    "U",
    "V",
    "W",
    "X",
    "Y",
    "Z",
    "LWIN",
    "RWIN",
    "APPS",
    "",
    "Sleep",
    "Num0",
    "Num1",
    "Num2",
    "Num3",
    "Num4",
    "Num5",
    "Num6",
    "Num7",
    "Num8",
    "Num9",
    "Num*",
    "Num+",
    "Num,",
    "Num-",
    "Num.",
    "Num/",
    "F1",
    "F2",
    "F3",
    "F4",
    "F5",
    "F6",
    "F7",
    "F8",
    "F9",
    "F10",
    "F11",
    "F12",
    "F13",
    "F14",
    "F15",
    "F16",
    "F17",
    "F18",
    "F19",
    "F20",
    "F21",
    "F22",
    "F23",
    "F24",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "NumLk",
    "ScrLk",
    "Oem1",
    "Oem2",
    "Oem3",
    "Oem4",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "LShift",
    "RShift",
    "LCTRL",
    "RCONTROL",
    "LALT",
    "RALT",
    "BrBack",
    "BrForward",
    "BrRefresh",
    "BrStop",
    "BrSearch",
    "BrFavorites",
    "BrHome",
    "VolMute",
    "VolDown",
    "VolUp",
    "NextTrk",
    "PrevTrk",
    "MdStop",
    "MdPlay",
    "Mail",
    "MdSelect",
    "App1",
    "App2",
    "",
    "",
    ";",
    "+",
    ",",
    "-",
    ".",
    "?",
    "~",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "[",
    "\\",
    "]",
    "\"",
    "Oem5",
    "",
    "Oem6",
    "Oem7",
    "Oem8",
    "Oem9",
    "Process",
    "Oem10",
    "",
    "",
    "Oem11",
    "Oem12",
    "Oem13",
    "Oem14",
    "Oem15",
    "Oem16",
    "Oem17",
    "Oem18",
    "Oem19",
    "Oem20",
    "Oem21",
    "Oem22",
    "Oem23",
    "Addn",
    "CrSel",
    "ExSel",
    "Ereof",
    "Play",
    "Zoom",
    "",
    "PA1",
    "Clear",
    ""
};

static char shortcutsDir[512];

extern Properties *properties;

typedef struct {
    unsigned type : 8;
    unsigned mods : 8;
    unsigned key  : 16;
} ShortcutHotkey;

struct IniFile
{
    char *iniBuffer;
    char *iniPtr;
    char *iniEnd;
    char *wrtBuffer;
    int   wrtBufferSize;
    int   wrtOffset;
    int   modified;
    char  iniFilename[512];
    char  zipFile[512];
    int   isZipped;
};

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
static const ShortcutHotkey resetHard = {
    HOTKEY_TYPE_KEYBOARD, 0, SDLK_F7,	
};
static const ShortcutHotkey screenShot = {
    HOTKEY_TYPE_KEYBOARD, 0, SDLK_F6,	
};
static const ShortcutHotkey toggleScanline = {
    HOTKEY_TYPE_KEYBOARD, KMOD_LALT, SDLK_F10,	
};
static const ShortcutHotkey toggleAspectRatio = {
    HOTKEY_TYPE_KEYBOARD, KMOD_LCTRL, SDLK_F10,	
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

    ShortcutHotkey diskChange[0];
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
	ShortcutHotkey scanlinesToggle;
    ShortcutHotkey aspectRatioToggle;
    struct {
        int maxSpeedIsSet;
    } state;
};


#define LOAD_SHORTCUT(hotkey) loadShortcut(iniFile, #hotkey, &shortcuts->hotkey)

#define HOTKEY_EQ(hotkey1, hotkey2) (*(UInt32*)&hotkey1 == *(UInt32*)&hotkey2)

ShortcutHotkey toSDLhotkey(ShortcutHotkey hotkey)
{
	int sdlmod = 0;
	int key = 0;
	switch (hotkey.type)
	{
		case HOTKEY_TYPE_KEYBOARD:
			if (hotkey.mods & KBD_LCTRL)   sdlmod |= KMOD_LCTRL;
			if (hotkey.mods & KBD_RCTRL)   sdlmod |= KMOD_RCTRL;
			if (hotkey.mods & KBD_LSHIFT)  sdlmod |= KMOD_LSHIFT;
			if (hotkey.mods & KBD_RSHIFT)  sdlmod |= KMOD_RSHIFT;
			if (hotkey.mods & KBD_LALT)    sdlmod |= KMOD_LALT;
			if (hotkey.mods & KBD_RALT)    sdlmod |= KMOD_RALT;
			if (hotkey.mods & KBD_LWIN)    sdlmod |= KMOD_LMETA;
			if (hotkey.mods & KBD_RWIN)    sdlmod |= KMOD_RMETA; 
			key = sdlkeys[hotkey.key];
			break;
		case HOTKEY_TYPE_JOYSTICK:
			break;
	}
	hotkey.mods = sdlmod;
	hotkey.key = key;
	return hotkey;
}

char* shortcutsToString(ShortcutHotkey hotkey) 
{
    static char buf[64];
        
    buf[0] = 0;

    switch (hotkey.type) {
    case HOTKEY_TYPE_KEYBOARD:
        if (hotkey.mods & KBD_LCTRL)    { strcat(buf, *buf ? "+" : ""); strcat(buf, "LCtrl"); }
        if (hotkey.mods & KBD_RCTRL)    { strcat(buf, *buf ? "+" : ""); strcat(buf, "RCtrl"); }
        if (hotkey.mods & KBD_LSHIFT)   { strcat(buf, *buf ? "+" : ""); strcat(buf, "LShift"); }
        if (hotkey.mods & KBD_RSHIFT)   { strcat(buf, *buf ? "+" : ""); strcat(buf, "RShift"); }
        if (hotkey.mods & KBD_LALT)     { strcat(buf, *buf ? "+" : ""); strcat(buf, "LAlt"); }
        if (hotkey.mods & KBD_RALT)     { strcat(buf, *buf ? "+" : ""); strcat(buf, "RAlt"); }
        if (hotkey.mods & KBD_LWIN)     { strcat(buf, *buf ? "+" : ""); strcat(buf, "LWin"); }
        if (hotkey.mods & KBD_RWIN)     { strcat(buf, *buf ? "+" : ""); strcat(buf, "RWin"); }
        if (virtualKeys[hotkey.key][0]) { strcat(buf, *buf ? "+" : ""); strcat(buf, virtualKeys[hotkey.key]); }
        break;
    case HOTKEY_TYPE_JOYSTICK:
        sprintf(buf, "%s %d", "JoyBt", hotkey.key);
        break;
    }

    return buf;
}

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

static ShortcutHotkey int2hotkey(int* hotkey) {
    return *(ShortcutHotkey*)hotkey;
}

static void loadShortcut(IniFile *iniFile, char* name, ShortcutHotkey* hotkey)
{
    char buffer[512];
    char* token, *value;
    int key;
    
    hotkey->type = HOTKEY_TYPE_NONE;
    hotkey->mods = 0;
    hotkey->key  = 0;

    if (!iniFileGetString(iniFile, "Shortcuts", name, "", buffer, sizeof(buffer))) {
//		printf("shortcut:%s - not assigned\n", name);
        return;
    }
// #if 0
    // token = strtok(buffer, "|");
    // if (token == NULL) {
        // return;
    // }
    // key = stringToHotkey(token);
    // if (key >= 0) {
        // hotkey->key  = key;
        // hotkey->type = HOTKEY_TYPE_KEYBOARD;
    // }
    
    // while (token = strtok(NULL, "|")) {
        // hotkey->mods |= stringToMod(token);
    // }
// #else
//	sscanf(strtok(buffer, "="), "%"SCNx32, hotkey);	
    sscanf(buffer, "%X", hotkey);
	toSDLhotkey(*hotkey);
//#endifma	
	printf("shortcut:%s - %s,%08x -> %08x\n", name, shortcutsToString(*hotkey), *hotkey, toSDLhotkey(*hotkey));
	*hotkey = toSDLhotkey(*hotkey);
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
	
	printf("shortcut file:%s, %d\n", filename, iniFile);

	if (iniFile->iniBuffer) {
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
		LOAD_SHORTCUT(diskChange[0]);
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
		LOAD_SHORTCUT(captureScreenshot);
		LOAD_SHORTCUT(scanlinesToggle);
		LOAD_SHORTCUT(aspectRatioToggle);
	}
	iniFileClose(iniFile);
	shortcuts->resetSoft = resetHard;
	shortcuts->captureScreenshot = screenShot;
	shortcuts->scanlinesToggle = toggleScanline;
	shortcuts->aspectRatioToggle = toggleAspectRatio;
	
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
	printf("key=%08x\n", key);
    if (s->state.maxSpeedIsSet) {
        actionMaxSpeedRelease();
        s->state.maxSpeedIsSet = 0;
    }

    if (HOTKEY_EQ(key, quitHotKey)) actionQuit();

    if (HOTKEY_EQ(key, disableFrameskipHotKey)) {
        properties->video.frameSkip = 0;
    }
    if (HOTKEY_EQ(key, lowFrameskipHotKey)) {
//        properties->video.frameSkip = 1;
    }
    if (HOTKEY_EQ(key, mediumFrameskipHotKey)) {
//       properties->video.frameSkip = 2;
    }
    if (HOTKEY_EQ(key, highFrameskipHotKey)) {
//        properties->video.frameSkip = 3;
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

    if (HOTKEY_EQ(key, s->diskChange[0]))              	 actionDiskQuickChange();
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
	if (HOTKEY_EQ(key, s->scanlinesToggle))	 			 actionToggleScanlinesEnable();
	if (HOTKEY_EQ(key, s->aspectRatioToggle))	 		 actionToggleVideoSetForce4x3ratio();
}


