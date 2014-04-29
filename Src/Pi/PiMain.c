/*****************************************************************************
**
** Blueberry Pi
** https://github.com/Melllvar/Blueberry-Pi
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

#include <stdlib.h>
#include <stdio.h>
#include <SDL.h>

#include "CommandLine.h"
#include "Properties.h"
#include "ArchFile.h"
#include "VideoRender.h"
#include "AudioMixer.h"
#include "Casette.h"
#include "PrinterIO.h"
#include "UartIO.h"
#include "MidiIO.h"
#include "Machine.h"
#include "Board.h"
#include "Emulator.h"
#include "FileHistory.h"
#include "InputEvent.h"
#include "Actions.h"
#include "Language.h"
#include "LaunchFile.h"
#include "ArchEvent.h"
#include "ArchSound.h"
#include "ArchNotifications.h"
#include "JoystickPort.h"
#include "PiShortcuts.h"
#include "PiVideo.h"

#define EVENT_UPDATE_DISPLAY 2

static void setDefaultPaths(const char* rootDir);
static void handleEvent(SDL_Event* event);

Video *video;
Properties *properties;
static Mixer *mixer;

static Shortcuts* shortcuts;
static int doQuit = 0;

static int pendingDisplayEvents = 0;
static void *dpyUpdateAckEvent = NULL;

#define JOYSTICK_COUNT 2
static SDL_Joystick *joysticks[JOYSTICK_COUNT];

int archUpdateEmuDisplay(int syncMode) 
{
	SDL_Event event;
	if (pendingDisplayEvents > 1) {
		return 1;
	}

	pendingDisplayEvents++;

	event.type = SDL_USEREVENT;
	event.user.code = EVENT_UPDATE_DISPLAY;
	event.user.data1 = NULL;
	event.user.data2 = NULL;

	SDL_PushEvent(&event);

	if (properties->emulation.syncMethod == P_EMU_SYNCFRAMES) {
		archEventWait(dpyUpdateAckEvent, 500);
	}

	return 1;
}

void archUpdateWindow()
{
}

void archEmulationStartNotification()
{
}

void archEmulationStopNotification()
{
}

void archEmulationStartFailure()
{
}

void archTrap(UInt8 value)
{
}

void archQuit()
{
	doQuit = 1;
}

static void handleEvent(SDL_Event* event) 
{
	switch (event->type) {
	case SDL_USEREVENT:
		switch (event->user.code) {
		case EVENT_UPDATE_DISPLAY:
			piUpdateEmuDisplay();
			archEventSet(dpyUpdateAckEvent);
			pendingDisplayEvents--;
			break;
		}
		break;
	case SDL_JOYBUTTONDOWN:
		if (event->jbutton.which == 0) {
			if (event->jbutton.button == 0) {
				inputEventSet(EC_JOY1_BUTTON1);
			} else if (event->jbutton.button == 1) {
				inputEventSet(EC_JOY1_BUTTON2);
			}
		} else if (event->jbutton.which == 1) {
			if (event->jbutton.button == 0) {
				inputEventSet(EC_JOY2_BUTTON1);
			} else if (event->jbutton.button == 1) {
				inputEventSet(EC_JOY2_BUTTON2);
			}
		}
		break;
	case SDL_JOYBUTTONUP:
		if (event->jbutton.which == 0) {
			if (event->jbutton.button == 0) {
				inputEventUnset(EC_JOY1_BUTTON1);
			} else if (event->jbutton.button == 1) {
				inputEventUnset(EC_JOY1_BUTTON2);
			}
		} else if (event->jbutton.which == 1) {
			if (event->jbutton.button == 0) {
				inputEventUnset(EC_JOY2_BUTTON1);
			} else if (event->jbutton.button == 1) {
				inputEventUnset(EC_JOY2_BUTTON2);
			}
		}
		break;
	case SDL_JOYAXISMOTION:
		if (event->jaxis.which == 0) {
			if (event->jaxis.axis == 0) {
				// Left/right
				if (event->jaxis.value < -3200) {
					inputEventSet(EC_JOY1_LEFT);
					inputEventUnset(EC_JOY1_RIGHT);
				} else if (event->jaxis.value > 3200) {
					inputEventUnset(EC_JOY1_LEFT);
					inputEventSet(EC_JOY1_RIGHT);
				} else {
					inputEventUnset(EC_JOY1_RIGHT);
					inputEventUnset(EC_JOY1_LEFT);
				}
			} else if (event->jaxis.axis == 1) {
				// Up/down
				if (event->jaxis.value < -3200) {
					inputEventSet(EC_JOY1_UP);
					inputEventUnset(EC_JOY1_DOWN);
				} else if (event->jaxis.value > 3200) {
					inputEventUnset(EC_JOY1_UP);
					inputEventSet(EC_JOY1_DOWN);
				} else {
					inputEventUnset(EC_JOY1_UP);
					inputEventUnset(EC_JOY1_DOWN);
				}
			}
		} else if (event->jaxis.which == 1) {
			if (event->jaxis.axis == 0) {
				// Left/right
				if (event->jaxis.value < -3200) {
					inputEventSet(EC_JOY2_LEFT);
					inputEventUnset(EC_JOY2_RIGHT);
				} else if (event->jaxis.value > 3200) {
					inputEventUnset(EC_JOY2_LEFT);
					inputEventSet(EC_JOY2_RIGHT);
				} else {
					inputEventUnset(EC_JOY2_RIGHT);
					inputEventUnset(EC_JOY2_LEFT);
				}
			} else if (event->jaxis.axis == 1) {
				// Up/down
				if (event->jaxis.value < -3200) {
					inputEventSet(EC_JOY2_UP);
					inputEventUnset(EC_JOY2_DOWN);
				} else if (event->jaxis.value > 3200) {
					inputEventUnset(EC_JOY2_UP);
					inputEventSet(EC_JOY2_DOWN);
				} else {
					inputEventUnset(EC_JOY2_UP);
					inputEventUnset(EC_JOY2_DOWN);
				}
			}
		}
		break;
	case SDL_ACTIVEEVENT:
		if (event->active.state & SDL_APPINPUTFOCUS) {
			keyboardSetFocus(1, event->active.gain);
		}
		break;
	case SDL_KEYDOWN:
		shortcutCheckDown(shortcuts, HOTKEY_TYPE_KEYBOARD, keyboardGetModifiers(), event->key.keysym.sym);
		break;
	case SDL_KEYUP:
		shortcutCheckUp(shortcuts, HOTKEY_TYPE_KEYBOARD, keyboardGetModifiers(), event->key.keysym.sym);
		break;
	}
}

static void setDefaultPaths(const char* rootDir)
{
	char buffer[512]; // FIXME

	propertiesSetDirectory(rootDir, rootDir);

	sprintf(buffer, "%s/Audio Capture", rootDir);
	archCreateDirectory(buffer);
	actionSetAudioCaptureSetDirectory(buffer, "");

	sprintf(buffer, "%s/Video Capture", rootDir);
	archCreateDirectory(buffer);
	actionSetAudioCaptureSetDirectory(buffer, "");

	sprintf(buffer, "%s/QuickSave", rootDir);
	archCreateDirectory(buffer);
	actionSetQuickSaveSetDirectory(buffer, "");

	sprintf(buffer, "%s/SRAM", rootDir);
	archCreateDirectory(buffer);
	boardSetDirectory(buffer);

	sprintf(buffer, "%s/Casinfo", rootDir);
	archCreateDirectory(buffer);
	tapeSetDirectory(buffer, "");

	sprintf(buffer, "%s/Databases", rootDir);
	archCreateDirectory(buffer);
	mediaDbLoad(buffer);

	sprintf(buffer, "%s/Keyboard Config", rootDir);
	archCreateDirectory(buffer);
	keyboardSetDirectory(buffer);

	sprintf(buffer, "%s/Shortcut Profiles", rootDir);
	archCreateDirectory(buffer);
	shortcutsSetDirectory(buffer);

	sprintf(buffer, "%s/Machines", rootDir);
	archCreateDirectory(buffer);
	machineSetDirectory(buffer);
}

int main(int argc, char **argv)
{
	if (!piInitVideo()) {
		fprintf(stderr, "piInitVideo() failed");
		return 1;
	}

	SDL_Init(SDL_INIT_EVERYTHING);
	SDL_ShowCursor(SDL_DISABLE);
	SDL_JoystickEventState(SDL_ENABLE);

	SDL_Event event;
	char szLine[8192] = "";
	int resetProperties;
	char path[512] = "";
	int i;

	for (i = 1; i < argc; i++) {
		if (strchr(argv[i], ' ') != NULL && argv[i][0] != '\"') {
			strcat(szLine, "\"");
			strcat(szLine, argv[i]);
			strcat(szLine, "\"");
		} else {
			strcat(szLine, argv[i]);
		}
		strcat(szLine, " ");
	}

	for (i = 0; i < JOYSTICK_COUNT; i++) {
		joysticks[i] = SDL_JoystickOpen(i);
	}

	setDefaultPaths(archGetCurrentDirectory());

	resetProperties = emuCheckResetArgument(szLine);
	strcat(path, archGetCurrentDirectory());
	strcat(path, DIR_SEPARATOR "bluemsx.ini");

	properties = propCreate(resetProperties, 0, P_KBD_EUROPEAN, 0, "");

	if (resetProperties == 2) {
		propDestroy(properties);
		return 0;
	}

	video = videoCreate();
	videoSetPalMode(video, VIDEO_PAL_FAST);
	videoSetColors(video, properties->video.saturation,
		properties->video.brightness,
		properties->video.contrast, properties->video.gamma);
	videoSetScanLines(video, properties->video.scanlinesEnable,
		properties->video.scanlinesPct);
	videoSetColorSaturation(video, properties->video.colorSaturationEnable,
		properties->video.colorSaturationWidth);

	dpyUpdateAckEvent = archEventCreate(0);

	keyboardInit();

	mixer = mixerCreate();
	for (i = 0; i < MIXER_CHANNEL_TYPE_COUNT; i++) {
		mixerSetChannelTypeVolume(mixer, i,
			properties->sound.mixerChannel[i].volume);
		mixerSetChannelTypePan(mixer, i,
			properties->sound.mixerChannel[i].pan);
		mixerEnableChannelType(mixer, i,
			properties->sound.mixerChannel[i].enable);
	}
	mixerSetMasterVolume(mixer, properties->sound.masterVolume);
	mixerEnableMaster(mixer, properties->sound.masterEnable);

	emulatorInit(properties, mixer);
	actionInit(video, properties, mixer);
	tapeSetReadOnly(properties->cassette.readOnly);

	langInit();
	langSetLanguage(properties->language);

	joystickPortSetType(0, properties->joy1.typeId);
	joystickPortSetType(1, properties->joy2.typeId);

	printerIoSetType(properties->ports.Lpt.type,
		properties->ports.Lpt.fileName);
	printerIoSetType(properties->ports.Lpt.type,
		properties->ports.Lpt.fileName);
	uartIoSetType(properties->ports.Com.type,
		properties->ports.Com.fileName);
	midiIoSetMidiOutType(properties->sound.MidiOut.type,
		properties->sound.MidiOut.fileName);
	midiIoSetMidiInType(properties->sound.MidiIn.type,
		properties->sound.MidiIn.fileName);
	ykIoSetMidiInType(properties->sound.YkIn.type,
		properties->sound.YkIn.fileName);

	emulatorRestartSound();

	videoUpdateAll(video, properties);

	shortcuts = shortcutsCreate();

	mediaDbSetDefaultRomType(properties->cartridge.defaultType);

	for (i = 0; i < PROP_MAX_CARTS; i++) {
		if (properties->media.carts[i].fileName[0]) {
			insertCartridge(properties, i,
				properties->media.carts[i].fileName,
				properties->media.carts[i].fileNameInZip,
				properties->media.carts[i].type, -1);
		}
		updateExtendedRomName(i,
			properties->media.carts[i].fileName,
			properties->media.carts[i].fileNameInZip);
	}

	for (i = 0; i < PROP_MAX_DISKS; i++) {
		if (properties->media.disks[i].fileName[0]) {
			insertDiskette(properties, i,
				properties->media.disks[i].fileName,
				properties->media.disks[i].fileNameInZip, -1);
		}
		updateExtendedDiskName(i,
			properties->media.disks[i].fileName,
			properties->media.disks[i].fileNameInZip);
	}

	for (i = 0; i < PROP_MAX_TAPES; i++) {
		if (properties->media.tapes[i].fileName[0]) {
			insertCassette(properties, i,
				properties->media.tapes[i].fileName,
				properties->media.tapes[i].fileNameInZip, 0);
		}
		updateExtendedCasName(i,
			properties->media.tapes[i].fileName,
			properties->media.tapes[i].fileNameInZip);
	}

	{
		Machine* machine = machineCreate(properties->emulation.machineName);
		if (machine != NULL) {
			boardSetMachine(machine);
			machineDestroy(machine);
		} else {
			fprintf(stderr, "Error creating machine\n");
			return 1;
		}
	}

	boardSetFdcTimingEnable(properties->emulation.enableFdcTiming);
	boardSetY8950Enable(properties->sound.chip.enableY8950);
	boardSetYm2413Enable(properties->sound.chip.enableYM2413);
	boardSetMoonsoundEnable(properties->sound.chip.enableMoonsound);
	boardSetVideoAutodetect(properties->video.detectActiveMonitor);

	i = emuTryStartWithArguments(properties, szLine, NULL);
	if (i < 0) {
		fprintf(stderr, "Failed to parse command line\n");
		return 1;
	}

	if (i == 0) {
		emulatorStart(NULL);
	}

	fprintf(stderr, "Entering main loop\n");

	while (!doQuit) {
		SDL_WaitEvent(&event);
		do {
			if (event.type == SDL_QUIT ) {
				doQuit = 1;
			} else {
				handleEvent(&event);
			}
		} while (SDL_PollEvent(&event));
	}

	fprintf(stderr, "Exited main loop\n");

	SDL_Quit();

	videoDestroy(video);
	propDestroy(properties);
	archSoundDestroy();
	mixerDestroy(mixer);

	piDestroyVideo();

	return 0;
}
