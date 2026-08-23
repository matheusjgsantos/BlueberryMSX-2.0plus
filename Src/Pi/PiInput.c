/*****************************************************************************
** $Source: /cygdrive/d/Private/_SVNROOT/bluemsx/blueMSX/Src/Sdl/SdlInput.c,v $
**
** $Revision: 1.11 $
**
** $Date: 2008-03-31 19:42:23 $
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
#include "ArchInput.h"
#include "Language.h"
#include "Properties.h"
#include "InputEvent.h"
#include "JoystickPort.h"
#include <stdio.h>
#include <SDL.h>
#include <SDL_keyboard.h>
#include <linux/input.h>
#include <linux/input-event-codes.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

//static int kbdTable[3][SDLK_LAST];
static long kbdTable[3][SDL_NUM_SCANCODES];

static int inputTypeScanStart = 0;
static int inputTypeScanEnd = 1;

extern Properties *properties;

static void initKbdTable()
{
	memset (kbdTable, 0, sizeof(kbdTable));
	#define MAP(row, key, ec) kbdTable[row][key] = ec

	MAP(0, SDL_SCANCODE_0, EC_0);
	MAP(0, SDL_SCANCODE_1, EC_1);
	MAP(0, SDL_SCANCODE_2, EC_2);
	MAP(0, SDL_SCANCODE_3, EC_3);
	MAP(0, SDL_SCANCODE_4, EC_4);
	MAP(0, SDL_SCANCODE_5, EC_5);
	MAP(0, SDL_SCANCODE_6, EC_6);
	MAP(0, SDL_SCANCODE_7, EC_7);
	MAP(0, SDL_SCANCODE_8, EC_8);
	MAP(0, SDL_SCANCODE_9, EC_9);
	MAP(0, SDL_SCANCODE_MINUS, EC_NEG);
	MAP(0, SDL_SCANCODE_EQUALS, EC_CIRCFLX);
	MAP(0, SDL_SCANCODE_BACKSLASH, EC_BKSLASH);
	MAP(0, SDL_SCANCODE_LEFTBRACKET, EC_AT);
	MAP(0, SDL_SCANCODE_RIGHTBRACKET, EC_LBRACK);
	MAP(0, SDL_SCANCODE_SEMICOLON, EC_SEMICOL);
	MAP(0, SDL_SCANCODE_APOSTROPHE, EC_COLON);
	MAP(0, SDL_SCANCODE_COMMA, EC_COMMA);
	MAP(0, SDL_SCANCODE_PERIOD, EC_PERIOD);
	MAP(0, SDL_SCANCODE_SLASH, EC_DIV);
	MAP(0, SDL_SCANCODE_RCTRL, EC_UNDSCRE);
	MAP(0, SDL_SCANCODE_A, EC_A);
	MAP(0, SDL_SCANCODE_B, EC_B);
	MAP(0, SDL_SCANCODE_C, EC_C);
	MAP(0, SDL_SCANCODE_D, EC_D);
	MAP(0, SDL_SCANCODE_E, EC_E);
	MAP(0, SDL_SCANCODE_F, EC_F);
	MAP(0, SDL_SCANCODE_G, EC_G);
	MAP(0, SDL_SCANCODE_H, EC_H);
	MAP(0, SDL_SCANCODE_I, EC_I);
	MAP(0, SDL_SCANCODE_J, EC_J);
	MAP(0, SDL_SCANCODE_K, EC_K);
	MAP(0, SDL_SCANCODE_L, EC_L);
	MAP(0, SDL_SCANCODE_M, EC_M);
	MAP(0, SDL_SCANCODE_N, EC_N);
	MAP(0, SDL_SCANCODE_O, EC_O);
	MAP(0, SDL_SCANCODE_P, EC_P);
	MAP(0, SDL_SCANCODE_Q, EC_Q);
	MAP(0, SDL_SCANCODE_R, EC_R);
	MAP(0, SDL_SCANCODE_S, EC_S);
	MAP(0, SDL_SCANCODE_T, EC_T);
	MAP(0, SDL_SCANCODE_U, EC_U);
	MAP(0, SDL_SCANCODE_V, EC_V);
	MAP(0, SDL_SCANCODE_W, EC_W);
	MAP(0, SDL_SCANCODE_X, EC_X);
	MAP(0, SDL_SCANCODE_Y, EC_Y);
	MAP(0, SDL_SCANCODE_Z, EC_Z);
	MAP(0, SDL_SCANCODE_F1, EC_F1);
	MAP(0, SDL_SCANCODE_F2, EC_F2);
	MAP(0, SDL_SCANCODE_F3, EC_F3);
	MAP(0, SDL_SCANCODE_F4, EC_F4);
	MAP(0, SDL_SCANCODE_F5, EC_F5);
	MAP(0, SDL_SCANCODE_ESCAPE, EC_ESC);
	MAP(0, SDL_SCANCODE_TAB, EC_TAB);
	MAP(0, SDL_SCANCODE_PAGEUP, EC_STOP);
	MAP(0, SDL_SCANCODE_BACKSPACE, EC_BKSPACE);
	MAP(0, SDL_SCANCODE_END, EC_SELECT);
	MAP(0, SDL_SCANCODE_RETURN, EC_RETURN);
	MAP(0, SDL_SCANCODE_SPACE, EC_SPACE);
	MAP(0, SDL_SCANCODE_HOME, EC_CLS);
	MAP(0, SDL_SCANCODE_INSERT, EC_INS);
	MAP(0, SDL_SCANCODE_DELETE, EC_DEL);
	MAP(0, SDL_SCANCODE_LEFT, EC_LEFT);
	MAP(0, SDL_SCANCODE_UP, EC_UP);
	MAP(0, SDL_SCANCODE_RIGHT, EC_RIGHT);
	MAP(0, SDL_SCANCODE_DOWN, EC_DOWN);
	MAP(0, SDL_SCANCODE_KP_MULTIPLY, EC_NUMMUL);
	MAP(0, SDL_SCANCODE_KP_PLUS, EC_NUMADD);
	MAP(0, SDL_SCANCODE_KP_DIVIDE, EC_NUMDIV);
	MAP(0, SDL_SCANCODE_KP_MINUS, EC_NUMSUB);
	MAP(0, SDL_SCANCODE_KP_PERIOD, EC_NUMPER);
	MAP(0, SDL_SCANCODE_PAGEDOWN, EC_NUMCOM);
	MAP(0, SDL_SCANCODE_KP_0, EC_NUM0);
	MAP(0, SDL_SCANCODE_KP_1, EC_NUM1);
	MAP(0, SDL_SCANCODE_KP_2, EC_NUM2);
	MAP(0, SDL_SCANCODE_KP_3, EC_NUM3);
	MAP(0, SDL_SCANCODE_KP_4, EC_NUM4);
	MAP(0, SDL_SCANCODE_KP_5, EC_NUM5);
	MAP(0, SDL_SCANCODE_KP_6, EC_NUM6);
	MAP(0, SDL_SCANCODE_KP_7, EC_NUM7);
	MAP(0, SDL_SCANCODE_KP_8, EC_NUM8);
	MAP(0, SDL_SCANCODE_KP_9, EC_NUM9);
	MAP(0, SDL_SCANCODE_LGUI, EC_TORIKE);
	MAP(0, SDL_SCANCODE_RGUI, EC_JIKKOU);
	MAP(0, SDL_SCANCODE_LSHIFT, EC_LSHIFT);
	MAP(0, SDL_SCANCODE_RSHIFT, EC_RSHIFT);
	MAP(0, SDL_SCANCODE_LCTRL, EC_CTRL);
	MAP(0, SDL_SCANCODE_LALT, EC_GRAPH);
	MAP(0, SDL_SCANCODE_RALT, EC_CODE);
	MAP(0, SDL_SCANCODE_CAPSLOCK, EC_CAPS);
	MAP(0, SDL_SCANCODE_KP_ENTER, EC_PAUSE);
	MAP(0, SDL_SCANCODE_SYSREQ, EC_PRINT);
	MAP(1, SDL_SCANCODE_SPACE, EC_JOY1_BUTTON1);
	MAP(1, SDL_SCANCODE_LCTRL, EC_JOY1_BUTTON2);
	MAP(1, SDL_SCANCODE_LEFT, EC_JOY1_LEFT);
	MAP(1, SDL_SCANCODE_UP, EC_JOY1_UP);
	MAP(1, SDL_SCANCODE_RIGHT, EC_JOY1_RIGHT);
	MAP(1, SDL_SCANCODE_DOWN, EC_JOY1_DOWN);
	MAP(1, SDL_SCANCODE_0, EC_COLECO1_0);
	MAP(1, SDL_SCANCODE_1, EC_COLECO1_1);
	MAP(1, SDL_SCANCODE_2, EC_COLECO1_2);
	MAP(1, SDL_SCANCODE_3, EC_COLECO1_3);
	MAP(1, SDL_SCANCODE_4, EC_COLECO1_4);
	MAP(1, SDL_SCANCODE_5, EC_COLECO1_5);
	MAP(1, SDL_SCANCODE_6, EC_COLECO1_6);
	MAP(1, SDL_SCANCODE_7, EC_COLECO1_7);
	MAP(1, SDL_SCANCODE_8, EC_COLECO1_8);
	MAP(1, SDL_SCANCODE_9, EC_COLECO1_9);
	MAP(1, SDL_SCANCODE_MINUS, EC_COLECO1_STAR);
	MAP(1, SDL_SCANCODE_EQUALS, EC_COLECO1_HASH);
	MAP(2, SDL_SCANCODE_KP_0, EC_COLECO2_0);
	MAP(2, SDL_SCANCODE_KP_1, EC_COLECO2_1);
	MAP(2, SDL_SCANCODE_KP_2, EC_COLECO2_2);
	MAP(2, SDL_SCANCODE_KP_3, EC_COLECO2_3);
	MAP(2, SDL_SCANCODE_KP_4, EC_COLECO2_4);
	MAP(2, SDL_SCANCODE_KP_5, EC_COLECO2_5);
	MAP(2, SDL_SCANCODE_KP_6, EC_COLECO2_6);
	MAP(2, SDL_SCANCODE_KP_7, EC_COLECO2_7);
	MAP(2, SDL_SCANCODE_KP_8, EC_COLECO2_8);
	MAP(2, SDL_SCANCODE_KP_9, EC_COLECO2_9);
	MAP(2, SDL_SCANCODE_KP_MULTIPLY, EC_COLECO2_STAR);
	MAP(2, SDL_SCANCODE_KP_DIVIDE, EC_COLECO2_HASH);
	#undef MAP
}


void piInputResetJoysticks()
{
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	SDL_JoystickEventState(SDL_ENABLE);
	SDL_JoystickOpen(0);
	SDL_JoystickOpen(1);
}

void piInputResetMSXDevices(int realMice, int realJoysticks)
{
	int port = 0;
	// Any connected joysticks take priority
	while (realJoysticks > 0 && port < 2) {
		if (port == 0) {
			properties->joy1.typeId = JOYSTICK_PORT_JOYSTICK;
			joystickPortSetType(port, JOYSTICK_PORT_JOYSTICK);
		} else if (port == 1) {
			properties->joy2.typeId = JOYSTICK_PORT_JOYSTICK;
			joystickPortSetType(port, JOYSTICK_PORT_JOYSTICK);
		}
		
		realJoysticks--;
		port++;

		fprintf(stderr, "Connecting a joystick to port %d\n", port);
	}

	// If there are still open ports and a mouse,
	// connect it to the remaining port 
	if (realMice > 0) {
		if (port == 0) {
			properties->joy1.typeId = JOYSTICK_PORT_MOUSE;
			joystickPortSetType(port++, properties->joy1.typeId);
			fprintf(stderr, "Connecting a mouse to port 1\n");
		} else if (port == 1) {
			properties->joy2.typeId = JOYSTICK_PORT_MOUSE;
			joystickPortSetType(port++, properties->joy2.typeId);
			fprintf(stderr, "Connecting a mouse to port 2\n");
		}
		
		realMice--;
	}
}

void keyboardInit(Properties *properties)
{
	if (strncmp(properties->emulation.machineName, "COL", 3) == 0) {
		inputTypeScanStart = 1;
		inputTypeScanEnd = 2;
		fprintf(stderr, "Initializing ColecoVision input\n");
	}

	initKbdTable();
	inputEventReset();
}

void keyboardUpdate(SDL_KeyboardEvent *event)
{
	int i;
	SDL_Scancode sc;
	int down;
	sc = (event->keysym.scancode != 0) ? (SDL_Scancode) event->keysym.scancode : SDL_GetScancodeFromKey(event->keysym.sym);
	if (sc == SDL_SCANCODE_UNKNOWN || sc >= SDL_NUM_SCANCODES) return;
	down = (event->type == SDL_KEYDOWN);
	for (i = inputTypeScanStart; i <= inputTypeScanEnd; i++) {
		long code = kbdTable[i][sc];
		if (code) {
			if (down) inputEventSet(code);
			else inputEventUnset(code);
		}
	}
}

#define EVDEV_MAX_FDS 16
static int evdevFds[EVDEV_MAX_FDS];
static int evdevFdCount = 0;
static pthread_t evdevThreadId;
static volatile int evdevRunning = 0;

static SDL_Scancode evdevKeyToScancode(unsigned int key)
{
	switch (key) {
		case KEY_A: return SDL_SCANCODE_A;
		case KEY_B: return SDL_SCANCODE_B;
		case KEY_C: return SDL_SCANCODE_C;
		case KEY_D: return SDL_SCANCODE_D;
		case KEY_E: return SDL_SCANCODE_E;
		case KEY_F: return SDL_SCANCODE_F;
		case KEY_G: return SDL_SCANCODE_G;
		case KEY_H: return SDL_SCANCODE_H;
		case KEY_I: return SDL_SCANCODE_I;
		case KEY_J: return SDL_SCANCODE_J;
		case KEY_K: return SDL_SCANCODE_K;
		case KEY_L: return SDL_SCANCODE_L;
		case KEY_M: return SDL_SCANCODE_M;
		case KEY_N: return SDL_SCANCODE_N;
		case KEY_O: return SDL_SCANCODE_O;
		case KEY_P: return SDL_SCANCODE_P;
		case KEY_Q: return SDL_SCANCODE_Q;
		case KEY_R: return SDL_SCANCODE_R;
		case KEY_S: return SDL_SCANCODE_S;
		case KEY_T: return SDL_SCANCODE_T;
		case KEY_U: return SDL_SCANCODE_U;
		case KEY_V: return SDL_SCANCODE_V;
		case KEY_W: return SDL_SCANCODE_W;
		case KEY_X: return SDL_SCANCODE_X;
		case KEY_Y: return SDL_SCANCODE_Y;
		case KEY_Z: return SDL_SCANCODE_Z;
		case KEY_1: return SDL_SCANCODE_1;
		case KEY_2: return SDL_SCANCODE_2;
		case KEY_3: return SDL_SCANCODE_3;
		case KEY_4: return SDL_SCANCODE_4;
		case KEY_5: return SDL_SCANCODE_5;
		case KEY_6: return SDL_SCANCODE_6;
		case KEY_7: return SDL_SCANCODE_7;
		case KEY_8: return SDL_SCANCODE_8;
		case KEY_9: return SDL_SCANCODE_9;
		case KEY_0: return SDL_SCANCODE_0;
		case KEY_F1: return SDL_SCANCODE_F1;
		case KEY_F2: return SDL_SCANCODE_F2;
		case KEY_F3: return SDL_SCANCODE_F3;
		case KEY_F4: return SDL_SCANCODE_F4;
		case KEY_F5: return SDL_SCANCODE_F5;
		case KEY_F6: return SDL_SCANCODE_F6;
		case KEY_F7: return SDL_SCANCODE_F7;
		case KEY_F8: return SDL_SCANCODE_F8;
		case KEY_F9: return SDL_SCANCODE_F9;
		case KEY_F10: return SDL_SCANCODE_F10;
		case KEY_F11: return SDL_SCANCODE_F11;
		case KEY_F12: return SDL_SCANCODE_F12;
		case KEY_ENTER: return SDL_SCANCODE_RETURN;
		case KEY_KPENTER: return SDL_SCANCODE_KP_ENTER;
		case KEY_ESC: return SDL_SCANCODE_ESCAPE;
		case KEY_BACKSPACE: return SDL_SCANCODE_BACKSPACE;
		case KEY_TAB: return SDL_SCANCODE_TAB;
		case KEY_CAPSLOCK: return SDL_SCANCODE_CAPSLOCK;
		case KEY_LEFTSHIFT: return SDL_SCANCODE_LSHIFT;
		case KEY_RIGHTSHIFT: return SDL_SCANCODE_RSHIFT;
		case KEY_LEFTCTRL: return SDL_SCANCODE_LCTRL;
		case KEY_RIGHTCTRL: return SDL_SCANCODE_RCTRL;
		case KEY_LEFTALT: return SDL_SCANCODE_LALT;
		case KEY_RIGHTALT: return SDL_SCANCODE_RALT;
		case KEY_LEFTMETA: return SDL_SCANCODE_LGUI;
		case KEY_RIGHTMETA: return SDL_SCANCODE_RGUI;
		case KEY_SPACE: return SDL_SCANCODE_SPACE;
		case KEY_MINUS: return SDL_SCANCODE_MINUS;
		case KEY_EQUAL: return SDL_SCANCODE_EQUALS;
		case KEY_LEFTBRACE: return SDL_SCANCODE_LEFTBRACKET;
		case KEY_RIGHTBRACE: return SDL_SCANCODE_RIGHTBRACKET;
		case KEY_BACKSLASH: return SDL_SCANCODE_BACKSLASH;
		case KEY_SEMICOLON: return SDL_SCANCODE_SEMICOLON;
		case KEY_APOSTROPHE: return SDL_SCANCODE_APOSTROPHE;
		case KEY_GRAVE: return SDL_SCANCODE_GRAVE;
		case KEY_COMMA: return SDL_SCANCODE_COMMA;
		case KEY_DOT: return SDL_SCANCODE_PERIOD;
		case KEY_SLASH: return SDL_SCANCODE_SLASH;
		case KEY_UP: return SDL_SCANCODE_UP;
		case KEY_DOWN: return SDL_SCANCODE_DOWN;
		case KEY_LEFT: return SDL_SCANCODE_LEFT;
		case KEY_RIGHT: return SDL_SCANCODE_RIGHT;
		case KEY_HOME: return SDL_SCANCODE_HOME;
		case KEY_END: return SDL_SCANCODE_END;
		case KEY_PAGEUP: return SDL_SCANCODE_PAGEUP;
		case KEY_PAGEDOWN: return SDL_SCANCODE_PAGEDOWN;
		case KEY_INSERT: return SDL_SCANCODE_INSERT;
		case KEY_DELETE: return SDL_SCANCODE_DELETE;
		case KEY_KP1: return SDL_SCANCODE_KP_1;
		case KEY_KP2: return SDL_SCANCODE_KP_2;
		case KEY_KP3: return SDL_SCANCODE_KP_3;
		case KEY_KP4: return SDL_SCANCODE_KP_4;
		case KEY_KP5: return SDL_SCANCODE_KP_5;
		case KEY_KP6: return SDL_SCANCODE_KP_6;
		case KEY_KP7: return SDL_SCANCODE_KP_7;
		case KEY_KP8: return SDL_SCANCODE_KP_8;
		case KEY_KP9: return SDL_SCANCODE_KP_9;
		case KEY_KP0: return SDL_SCANCODE_KP_0;
		case KEY_KPDOT: return SDL_SCANCODE_KP_PERIOD;
		case KEY_KPASTERISK: return SDL_SCANCODE_KP_MULTIPLY;
		case KEY_KPSLASH: return SDL_SCANCODE_KP_DIVIDE;
		case KEY_KPPLUS: return SDL_SCANCODE_KP_PLUS;
		case KEY_KPMINUS: return SDL_SCANCODE_KP_MINUS;
		case KEY_SYSRQ: return SDL_SCANCODE_PRINTSCREEN;
		case KEY_PAUSE: return SDL_SCANCODE_PAUSE;
		default: return SDL_SCANCODE_UNKNOWN;
	}
}

static Uint16 evdevModState = 0;

static void evdevUpdateModifiers(unsigned int code, int value)
{
	switch (code) {
	case KEY_LEFTCTRL:   if (value) evdevModState |= KMOD_LCTRL; else evdevModState &= (Uint16)~KMOD_LCTRL; break;
	case KEY_RIGHTCTRL:  if (value) evdevModState |= KMOD_RCTRL; else evdevModState &= (Uint16)~KMOD_RCTRL; break;
	case KEY_LEFTSHIFT:  if (value) evdevModState |= KMOD_LSHIFT; else evdevModState &= (Uint16)~KMOD_LSHIFT; break;
	case KEY_RIGHTSHIFT: if (value) evdevModState |= KMOD_RSHIFT; else evdevModState &= (Uint16)~KMOD_RSHIFT; break;
	case KEY_LEFTALT:    if (value) evdevModState |= KMOD_LALT; else evdevModState &= (Uint16)~KMOD_LALT; break;
	case KEY_RIGHTALT:   if (value) evdevModState |= KMOD_RALT; else evdevModState &= (Uint16)~KMOD_RALT; break;
	case KEY_LEFTMETA:   if (value) evdevModState |= KMOD_LGUI; else evdevModState &= (Uint16)~KMOD_LGUI; break;
	case KEY_RIGHTMETA:  if (value) evdevModState |= KMOD_RGUI; else evdevModState &= (Uint16)~KMOD_RGUI; break;
	default: break;
	}
}

static SDL_Keycode evdevScancodeToKey(SDL_Scancode sc)
{
	switch (sc) {
	case SDL_SCANCODE_0: return SDLK_0;
	case SDL_SCANCODE_1: return SDLK_1;
	case SDL_SCANCODE_2: return SDLK_2;
	case SDL_SCANCODE_3: return SDLK_3;
	case SDL_SCANCODE_4: return SDLK_4;
	case SDL_SCANCODE_5: return SDLK_5;
	case SDL_SCANCODE_6: return SDLK_6;
	case SDL_SCANCODE_7: return SDLK_7;
	case SDL_SCANCODE_8: return SDLK_8;
	case SDL_SCANCODE_9: return SDLK_9;
	case SDL_SCANCODE_MINUS: return SDLK_MINUS;
	case SDL_SCANCODE_EQUALS: return SDLK_EQUALS;
	case SDL_SCANCODE_BACKSLASH: return SDLK_BACKSLASH;
	case SDL_SCANCODE_LEFTBRACKET: return SDLK_LEFTBRACKET;
	case SDL_SCANCODE_RIGHTBRACKET: return SDLK_RIGHTBRACKET;
	case SDL_SCANCODE_SEMICOLON: return SDLK_SEMICOLON;
	case SDL_SCANCODE_APOSTROPHE: return SDLK_QUOTE;
	case SDL_SCANCODE_COMMA: return SDLK_COMMA;
	case SDL_SCANCODE_PERIOD: return SDLK_PERIOD;
	case SDL_SCANCODE_SLASH: return SDLK_SLASH;
	case SDL_SCANCODE_A: return SDLK_a;
	case SDL_SCANCODE_B: return SDLK_b;
	case SDL_SCANCODE_C: return SDLK_c;
	case SDL_SCANCODE_D: return SDLK_d;
	case SDL_SCANCODE_E: return SDLK_e;
	case SDL_SCANCODE_F: return SDLK_f;
	case SDL_SCANCODE_G: return SDLK_g;
	case SDL_SCANCODE_H: return SDLK_h;
	case SDL_SCANCODE_I: return SDLK_i;
	case SDL_SCANCODE_J: return SDLK_j;
	case SDL_SCANCODE_K: return SDLK_k;
	case SDL_SCANCODE_L: return SDLK_l;
	case SDL_SCANCODE_M: return SDLK_m;
	case SDL_SCANCODE_N: return SDLK_n;
	case SDL_SCANCODE_O: return SDLK_o;
	case SDL_SCANCODE_P: return SDLK_p;
	case SDL_SCANCODE_Q: return SDLK_q;
	case SDL_SCANCODE_R: return SDLK_r;
	case SDL_SCANCODE_S: return SDLK_s;
	case SDL_SCANCODE_T: return SDLK_t;
	case SDL_SCANCODE_U: return SDLK_u;
	case SDL_SCANCODE_V: return SDLK_v;
	case SDL_SCANCODE_W: return SDLK_w;
	case SDL_SCANCODE_X: return SDLK_x;
	case SDL_SCANCODE_Y: return SDLK_y;
	case SDL_SCANCODE_Z: return SDLK_z;
	case SDL_SCANCODE_F1: return SDLK_F1;
	case SDL_SCANCODE_F2: return SDLK_F2;
	case SDL_SCANCODE_F3: return SDLK_F3;
	case SDL_SCANCODE_F4: return SDLK_F4;
	case SDL_SCANCODE_F5: return SDLK_F5;
	case SDL_SCANCODE_F6: return SDLK_F6;
	case SDL_SCANCODE_F7: return SDLK_F7;
	case SDL_SCANCODE_F8: return SDLK_F8;
	case SDL_SCANCODE_F9: return SDLK_F9;
	case SDL_SCANCODE_F10: return SDLK_F10;
	case SDL_SCANCODE_F11: return SDLK_F11;
	case SDL_SCANCODE_F12: return SDLK_F12;
	case SDL_SCANCODE_ESCAPE: return SDLK_ESCAPE;
	case SDL_SCANCODE_TAB: return SDLK_TAB;
	case SDL_SCANCODE_PAGEUP: return SDLK_PAGEUP;
	case SDL_SCANCODE_BACKSPACE: return SDLK_BACKSPACE;
	case SDL_SCANCODE_END: return SDLK_END;
	case SDL_SCANCODE_RETURN: return SDLK_RETURN;
	case SDL_SCANCODE_SPACE: return SDLK_SPACE;
	case SDL_SCANCODE_HOME: return SDLK_HOME;
	case SDL_SCANCODE_INSERT: return SDLK_INSERT;
	case SDL_SCANCODE_DELETE: return SDLK_DELETE;
	case SDL_SCANCODE_LEFT: return SDLK_LEFT;
	case SDL_SCANCODE_UP: return SDLK_UP;
	case SDL_SCANCODE_RIGHT: return SDLK_RIGHT;
	case SDL_SCANCODE_DOWN: return SDLK_DOWN;
	case SDL_SCANCODE_KP_MULTIPLY: return SDLK_KP_MULTIPLY;
	case SDL_SCANCODE_KP_PLUS: return SDLK_KP_PLUS;
	case SDL_SCANCODE_KP_DIVIDE: return SDLK_KP_DIVIDE;
	case SDL_SCANCODE_KP_MINUS: return SDLK_KP_MINUS;
	case SDL_SCANCODE_KP_PERIOD: return SDLK_KP_PERIOD;
	case SDL_SCANCODE_KP_ENTER: return SDLK_KP_ENTER;
	case SDL_SCANCODE_PAGEDOWN: return SDLK_PAGEDOWN;
	case SDL_SCANCODE_KP_0: return SDLK_KP_0;
	case SDL_SCANCODE_KP_1: return SDLK_KP_1;
	case SDL_SCANCODE_KP_2: return SDLK_KP_2;
	case SDL_SCANCODE_KP_3: return SDLK_KP_3;
	case SDL_SCANCODE_KP_4: return SDLK_KP_4;
	case SDL_SCANCODE_KP_5: return SDLK_KP_5;
	case SDL_SCANCODE_KP_6: return SDLK_KP_6;
	case SDL_SCANCODE_KP_7: return SDLK_KP_7;
	case SDL_SCANCODE_KP_8: return SDLK_KP_8;
	case SDL_SCANCODE_KP_9: return SDLK_KP_9;
	case SDL_SCANCODE_LGUI: return SDLK_LGUI;
	case SDL_SCANCODE_RGUI: return SDLK_RGUI;
	case SDL_SCANCODE_LSHIFT: return SDLK_LSHIFT;
	case SDL_SCANCODE_RSHIFT: return SDLK_RSHIFT;
	case SDL_SCANCODE_LCTRL: return SDLK_LCTRL;
	case SDL_SCANCODE_RCTRL: return SDLK_RCTRL;
	case SDL_SCANCODE_LALT: return SDLK_LALT;
	case SDL_SCANCODE_RALT: return SDLK_RALT;
	case SDL_SCANCODE_CAPSLOCK: return SDLK_CAPSLOCK;
	case SDL_SCANCODE_SYSREQ: return SDLK_SYSREQ;
	default: return SDLK_UNKNOWN;
	}
}

static void *evdevThreadMain(void *arg)
{
	struct input_event ev;
	int maxfd, fd, i;
	fd_set rfds;
	while (evdevRunning) {
		FD_ZERO(&rfds);
		maxfd = -1;
		for (i = 0; i < evdevFdCount; i++) {
			FD_SET(evdevFds[i], &rfds);
			if (evdevFds[i] > maxfd) maxfd = evdevFds[i];
		}
		if (maxfd < 0) break;
		if (select(maxfd + 1, &rfds, NULL, NULL, NULL) < 0) {
			if (errno == EINTR) continue;
			break;
		}
		for (fd = 0; fd < evdevFdCount; fd++) {
			while (FD_ISSET(evdevFds[fd], &rfds)) {
				ssize_t len;
				SDL_Event sev;
				SDL_Scancode sc;
				int j;
				len = read(evdevFds[fd], &ev, sizeof(ev));
				if (len != (ssize_t) sizeof(ev)) {
					if (len == 0 || errno == ENODEV || errno == ENOENT) {
						close(evdevFds[fd]);
						for (j = fd; j + 1 < evdevFdCount; j++) evdevFds[j] = evdevFds[j + 1];
						evdevFdCount--;
						fd--;
					}
					break;
				}
				if (ev.type == EV_KEY) evdevUpdateModifiers(ev.code, ev.value);
				if (ev.type != EV_KEY) continue;
				sc = evdevKeyToScancode(ev.code);
				if (sc == SDL_SCANCODE_UNKNOWN) continue;
				if (ev.value == 2) continue;
				memset(&sev, 0, sizeof(sev));
				sev.type = (ev.value == 1) ? SDL_KEYDOWN : SDL_KEYUP;
				sev.key.keysym.scancode = sc;
				sev.key.keysym.sym = evdevScancodeToKey(sc);
				sev.key.state = (ev.value == 1) ? SDL_PRESSED : SDL_RELEASED;
				sev.key.keysym.mod = evdevModState;
				sev.key.windowID = 1;
				SDL_PushEvent(&sev);
			}
		}
	}
	return NULL;
}

int piKeyboardEvdevInit(void)
{
	DIR *dir;
	struct dirent *ent;
	unsigned char bits[EV_MAX / 8 + 1];
	int found = 0;

	if (evdevFdCount > 0) return 0;
	dir = opendir("/dev/input");
	if (!dir) {
		fprintf(stderr, "evdev kbd: cannot open /dev/input: %s\n", strerror(errno));
		return -1;
	}
	while ((ent = readdir(dir)) != NULL && evdevFdCount < EVDEV_MAX_FDS) {
		int fd;
		char path[256];
		if (strncmp(ent->d_name, "event", 5) != 0) continue;
		snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);
		fd = open(path, O_RDONLY | O_NONBLOCK);
		if (fd < 0) continue;
		memset(bits, 0, sizeof(bits));
		if (ioctl(fd, EVIOCGBIT(0, sizeof(bits)), bits) < 0) {
			close(fd);
			continue;
		}
		if (!(bits[EV_KEY / 8] & (1 << (EV_KEY % 8)))) {
			close(fd);
			continue;
		}
		/* Grab the device exclusively: while we hold the grab the
		 * kernel delivers its events only to us, so keystrokes no
		 * longer leak to the console tty (no echo, no shell input,
		 * no Ctrl+C -> SIGINT). The grab is released automatically
		 * when the fd is closed (exit or crash). */
		if (ioctl(fd, EVIOCGRAB, 1) < 0) {
			fprintf(stderr, "evdev kbd: EVIOCGRAB %s failed: %s\n", path, strerror(errno));
		}
		evdevFds[evdevFdCount++] = fd;
		found++;
		fprintf(stderr, "evdev kbd: using %s\n", path);
	}
	closedir(dir);
	if (!found) {
		fprintf(stderr, "evdev kbd: no EV_KEY devices found\n");
		return -1;
	}
	evdevRunning = 1;
	if (pthread_create(&evdevThreadId, NULL, evdevThreadMain, NULL) != 0) {
		fprintf(stderr, "evdev kbd: pthread_create failed\n");
		evdevRunning = 0;
		return -1;
	}
	return 0;
}

void piKeyboardEvdevDestroy(void)
{
	int i;
	evdevRunning = 0;
	pthread_join(evdevThreadId, NULL);
	for (i = 0; i < evdevFdCount; i++) {
		ioctl(evdevFds[i], EVIOCGRAB, 0);
		close(evdevFds[i]);
	}
	evdevFdCount = 0;
}

void joystickAxisUpdate(SDL_JoyAxisEvent *event)
{
	if (event->which == 0) {
		if (event->axis == 0) {
			// Left/right
			if (event->value < -3200) {
				inputEventSet(EC_JOY1_LEFT);
				inputEventUnset(EC_JOY1_RIGHT);
			} else if (event->value > 3200) {
				inputEventUnset(EC_JOY1_LEFT);
				inputEventSet(EC_JOY1_RIGHT);
			} else {
				inputEventUnset(EC_JOY1_RIGHT);
				inputEventUnset(EC_JOY1_LEFT);
			}
		} else if (event->axis == 1) {
			// Up/down
			if (event->value < -3200) {
				inputEventSet(EC_JOY1_UP);
				inputEventUnset(EC_JOY1_DOWN);
			} else if (event->value > 3200) {
				inputEventUnset(EC_JOY1_UP);
				inputEventSet(EC_JOY1_DOWN);
			} else {
				inputEventUnset(EC_JOY1_UP);
				inputEventUnset(EC_JOY1_DOWN);
			}
		}
	} else if (event->which == 1) {
		if (event->axis == 0) {
			// Left/right
			if (event->value < -3200) {
				inputEventSet(EC_JOY2_LEFT);
				inputEventUnset(EC_JOY2_RIGHT);
			} else if (event->value > 3200) {
				inputEventUnset(EC_JOY2_LEFT);
				inputEventSet(EC_JOY2_RIGHT);
			} else {
				inputEventUnset(EC_JOY2_RIGHT);
				inputEventUnset(EC_JOY2_LEFT);
			}
		} else if (event->axis == 1) {
			// Up/down
			if (event->value < -3200) {
				inputEventSet(EC_JOY2_UP);
				inputEventUnset(EC_JOY2_DOWN);
			} else if (event->value > 3200) {
				inputEventUnset(EC_JOY2_UP);
				inputEventSet(EC_JOY2_DOWN);
			} else {
				inputEventUnset(EC_JOY2_UP);
				inputEventUnset(EC_JOY2_DOWN);
			}
		}
	}
}

void joystickButtonUpdate(SDL_JoyButtonEvent *event)
{
	if (event->type == SDL_JOYBUTTONDOWN) {
		if (event->which == 0) {
			if (event->button == 0) {
				inputEventSet(EC_JOY1_BUTTON1);
			} else if (event->button == 1) {
				inputEventSet(EC_JOY1_BUTTON2);
			}
		} else if (event->which == 1) {
			if (event->button == 0) {
				inputEventSet(EC_JOY2_BUTTON1);
			} else if (event->button == 1) {
				inputEventSet(EC_JOY2_BUTTON2);
			}
		}
	} else if (event->type == SDL_JOYBUTTONUP) {
		if (event->which == 0) {
			if (event->button == 0) {
				inputEventUnset(EC_JOY1_BUTTON1);
			} else if (event->button == 1) {
				inputEventUnset(EC_JOY1_BUTTON2);
			}
		} else if (event->which == 1) {
			if (event->button == 0) {
				inputEventUnset(EC_JOY2_BUTTON1);
			} else if (event->button == 1) {
				inputEventUnset(EC_JOY2_BUTTON2);
			}
		}
	}
}

void  archUpdateJoystick() {}
UInt8 archJoystickGetState(int joystickNo) { return 0; }
int   archJoystickGetCount() { return 0; }
char* archJoystickGetName(int index) { return ""; }
void  archMouseSetForceLock(int lock) { }
void  archPollInput() { }
void  archKeyboardSetSelectedKey(int keyCode) {}
char* archGetSelectedKey() { return ""; }
char* archGetMappedKey() { return ""; }
int   archKeyboardIsKeyConfigured(int msxKeyCode) { return 0; }
int   archKeyboardIsKeySelected(int msxKeyCode) { return 0; }
char* archKeyconfigSelectedKeyTitle() { return ""; }
char* archKeyconfigMappedToTitle() { return ""; }
char* archKeyconfigMappingSchemeTitle() { return ""; }
