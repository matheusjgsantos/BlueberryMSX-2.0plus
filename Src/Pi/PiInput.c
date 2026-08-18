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

//static int kbdTable[3][SDLK_LAST];
static long kbdTable[3][SDL_NUM_SCANCODES];

static int inputTypeScanStart = 0;
static int inputTypeScanEnd = 1;

extern Properties *properties;

static void initKbdTable()
{
	memset (kbdTable, 0, sizeof(kbdTable));
	#define MAP(row, key, ec) do { \
		SDL_Scancode sc = SDL_GetScancodeFromKey(key); \
		if(sc != SDL_SCANCODE_UNKNOWN) kbdTable[row][sc] = ec; \
	} while(0)

	MAP(0, SDLK_0, EC_0);
	MAP(0, SDLK_1, EC_1);
	MAP(0, SDLK_2, EC_2);
	MAP(0, SDLK_3, EC_3);
	MAP(0, SDLK_4, EC_4);
	MAP(0, SDLK_5, EC_5);
	MAP(0, SDLK_6, EC_6);
	MAP(0, SDLK_7, EC_7);
	MAP(0, SDLK_8, EC_8);
	MAP(0, SDLK_9, EC_9);
	MAP(0, SDLK_MINUS, EC_NEG);
	MAP(0, SDLK_EQUALS, EC_CIRCFLX);
	MAP(0, SDLK_BACKSLASH, EC_BKSLASH);
	MAP(0, SDLK_LEFTBRACKET, EC_AT);
	MAP(0, SDLK_RIGHTBRACKET, EC_LBRACK);
	MAP(0, SDLK_SEMICOLON, EC_SEMICOL);
	MAP(0, SDLK_QUOTE, EC_COLON);
	MAP(0, SDLK_CARET, EC_RBRACK);
	MAP(0, SDLK_COMMA, EC_COMMA);
	MAP(0, SDLK_PERIOD, EC_PERIOD);
	MAP(0, SDLK_SLASH, EC_DIV);
	MAP(0, SDLK_RCTRL, EC_UNDSCRE);
	MAP(0, SDLK_a, EC_A);
	MAP(0, SDLK_b, EC_B);
	MAP(0, SDLK_c, EC_C);
	MAP(0, SDLK_d, EC_D);
	MAP(0, SDLK_e, EC_E);
	MAP(0, SDLK_f, EC_F);
	MAP(0, SDLK_g, EC_G);
	MAP(0, SDLK_h, EC_H);
	MAP(0, SDLK_i, EC_I);
	MAP(0, SDLK_j, EC_J);
	MAP(0, SDLK_k, EC_K);
	MAP(0, SDLK_l, EC_L);
	MAP(0, SDLK_m, EC_M);
	MAP(0, SDLK_n, EC_N);
	MAP(0, SDLK_o, EC_O);
	MAP(0, SDLK_p, EC_P);
	MAP(0, SDLK_q, EC_Q);
	MAP(0, SDLK_r, EC_R);
	MAP(0, SDLK_s, EC_S);
	MAP(0, SDLK_t, EC_T);
	MAP(0, SDLK_u, EC_U);
	MAP(0, SDLK_v, EC_V);
	MAP(0, SDLK_w, EC_W);
	MAP(0, SDLK_x, EC_X);
	MAP(0, SDLK_y, EC_Y);
	MAP(0, SDLK_z, EC_Z);
	MAP(0, SDLK_F1, EC_F1);
	MAP(0, SDLK_F2, EC_F2);
	MAP(0, SDLK_F3, EC_F3);
	MAP(0, SDLK_F4, EC_F4);
	MAP(0, SDLK_F5, EC_F5);
	MAP(0, SDLK_ESCAPE, EC_ESC);
	MAP(0, SDLK_TAB, EC_TAB);
	MAP(0, SDLK_PAGEUP, EC_STOP);
	MAP(0, SDLK_BACKSPACE, EC_BKSPACE);
	MAP(0, SDLK_END, EC_SELECT);
	MAP(0, SDLK_RETURN, EC_RETURN);
	MAP(0, SDLK_SPACE, EC_SPACE);
	MAP(0, SDLK_HOME, EC_CLS);
	MAP(0, SDLK_INSERT, EC_INS);
	MAP(0, SDLK_DELETE, EC_DEL);
	MAP(0, SDLK_LEFT, EC_LEFT);
	MAP(0, SDLK_UP, EC_UP);
	MAP(0, SDLK_RIGHT, EC_RIGHT);
	MAP(0, SDLK_DOWN, EC_DOWN);
	MAP(0, SDLK_KP_MULTIPLY, EC_NUMMUL);
	MAP(0, SDLK_KP_PLUS, EC_NUMADD);
	MAP(0, SDLK_KP_DIVIDE, EC_NUMDIV);
	MAP(0, SDLK_KP_MINUS, EC_NUMSUB);
	MAP(0, SDLK_KP_PERIOD, EC_NUMPER);
	MAP(0, SDLK_PAGEDOWN, EC_NUMCOM);
	MAP(0, SDLK_KP_0, EC_NUM0);
	MAP(0, SDLK_KP_1, EC_NUM1);
	MAP(0, SDLK_KP_2, EC_NUM2);
	MAP(0, SDLK_KP_3, EC_NUM3);
	MAP(0, SDLK_KP_4, EC_NUM4);
	MAP(0, SDLK_KP_5, EC_NUM5);
	MAP(0, SDLK_KP_6, EC_NUM6);
	MAP(0, SDLK_KP_7, EC_NUM7);
	MAP(0, SDLK_KP_8, EC_NUM8);
	MAP(0, SDLK_KP_9, EC_NUM9);
	MAP(0, SDLK_LGUI, EC_TORIKE);
	MAP(0, SDLK_RGUI, EC_JIKKOU);
	MAP(0, SDLK_LSHIFT, EC_LSHIFT);
	MAP(0, SDLK_RSHIFT, EC_RSHIFT);
	MAP(0, SDLK_LCTRL, EC_CTRL);
	MAP(0, SDLK_LALT, EC_GRAPH);
	MAP(0, SDLK_RALT, EC_CODE);
	MAP(0, SDLK_CAPSLOCK, EC_CAPS);
	MAP(0, SDLK_KP_ENTER, EC_PAUSE);
	MAP(0, SDLK_SYSREQ, EC_PRINT);
	MAP(1, SDLK_SPACE, EC_JOY1_BUTTON1);
	MAP(1, SDLK_LCTRL, EC_JOY1_BUTTON2);
	MAP(1, SDLK_LEFT, EC_JOY1_LEFT);
	MAP(1, SDLK_UP, EC_JOY1_UP);
	MAP(1, SDLK_RIGHT, EC_JOY1_RIGHT);
	MAP(1, SDLK_DOWN, EC_JOY1_DOWN);
	MAP(1, SDLK_0, EC_COLECO1_0);
	MAP(1, SDLK_1, EC_COLECO1_1);
	MAP(1, SDLK_2, EC_COLECO1_2);
	MAP(1, SDLK_3, EC_COLECO1_3);
	MAP(1, SDLK_4, EC_COLECO1_4);
	MAP(1, SDLK_5, EC_COLECO1_5);
	MAP(1, SDLK_6, EC_COLECO1_6);
	MAP(1, SDLK_7, EC_COLECO1_7);
	MAP(1, SDLK_8, EC_COLECO1_8);
	MAP(1, SDLK_9, EC_COLECO1_9);
	MAP(1, SDLK_MINUS, EC_COLECO1_STAR);
	MAP(1, SDLK_EQUALS, EC_COLECO1_HASH);
	MAP(2, SDLK_KP_0, EC_COLECO2_0);
	MAP(2, SDLK_KP_1, EC_COLECO2_1);
	MAP(2, SDLK_KP_2, EC_COLECO2_2);
	MAP(2, SDLK_KP_3, EC_COLECO2_3);
	MAP(2, SDLK_KP_4, EC_COLECO2_4);
	MAP(2, SDLK_KP_5, EC_COLECO2_5);
	MAP(2, SDLK_KP_6, EC_COLECO2_6);
	MAP(2, SDLK_KP_7, EC_COLECO2_7);
	MAP(2, SDLK_KP_8, EC_COLECO2_8);
	MAP(2, SDLK_KP_9, EC_COLECO2_9);
	MAP(2, SDLK_KP_MULTIPLY, EC_COLECO2_STAR);
	MAP(2, SDLK_KP_DIVIDE, EC_COLECO2_HASH);
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
	for (i = inputTypeScanStart; i <= inputTypeScanEnd; i++) {
		sc = SDL_GetScancodeFromKey(event->keysym.sym);
		if (sc == SDL_SCANCODE_UNKNOWN) continue;
		if (event->keysym.scancode == 0x7A) {
			if (event->type == SDL_KEYUP) 
				inputEventUnset(kbdTable[i][SDL_GetScancodeFromKey(SDLK_RALT)]);
			else if (event->type == SDL_KEYDOWN)
				inputEventSet(kbdTable[i][SDL_GetScancodeFromKey(SDLK_RALT)]);
		} else if (event->type == SDL_KEYUP) {
			if (event->keysym.scancode == 0x2e) {
				inputEventUnset(EC_CIRCFLX);
			} else if (event->keysym.scancode == 0x3b) {
				inputEventUnset(EC_F2);
			} else {
				long code = kbdTable[i][sc];
				if (code) inputEventUnset(code);
			}
		} else if (event->type == SDL_KEYDOWN) {
			if (event->keysym.scancode == 0x2e) {
				inputEventSet(EC_CIRCFLX);
			} else if (event->keysym.scancode == 0x3b) {
				inputEventSet(EC_F2);
			} else {
				long code = kbdTable[i][sc];
				if (code) inputEventSet(code);
			}
		}
	}
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
