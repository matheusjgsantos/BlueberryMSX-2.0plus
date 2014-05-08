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
#include "InputEvent.h"
#include <stdio.h>
#include <SDL.h>

static int kbdTable[SDLK_LAST];

static void initKbdTable()
{
	memset (kbdTable, 0, sizeof(kbdTable));

	kbdTable[SDLK_0] = EC_0;
	kbdTable[SDLK_1] = EC_1;
	kbdTable[SDLK_2] = EC_2;
	kbdTable[SDLK_3] = EC_3;
	kbdTable[SDLK_4] = EC_4;
	kbdTable[SDLK_5] = EC_5;
	kbdTable[SDLK_6] = EC_6;
	kbdTable[SDLK_7] = EC_7;
	kbdTable[SDLK_8] = EC_8;
	kbdTable[SDLK_9] = EC_9;

	kbdTable[SDLK_MINUS       ] = EC_NEG;
	kbdTable[SDLK_EQUALS      ] = EC_CIRCFLX;
	kbdTable[SDLK_BACKSLASH   ] = EC_BKSLASH;
	kbdTable[SDLK_LEFTBRACKET ] = EC_AT;
	kbdTable[SDLK_RIGHTBRACKET] = EC_LBRACK;
	kbdTable[SDLK_SEMICOLON   ] = EC_SEMICOL;
	kbdTable[SDLK_QUOTE       ] = EC_COLON;
	kbdTable[SDLK_CARET       ] = EC_RBRACK;
	kbdTable[SDLK_COMMA       ] = EC_COMMA;
	kbdTable[SDLK_PERIOD      ] = EC_PERIOD;
	kbdTable[SDLK_SLASH       ] = EC_DIV;
	kbdTable[SDLK_RCTRL       ] = EC_UNDSCRE;

	kbdTable[SDLK_a] = EC_A;
	kbdTable[SDLK_b] = EC_B;
	kbdTable[SDLK_c] = EC_C;
	kbdTable[SDLK_d] = EC_D;
	kbdTable[SDLK_e] = EC_E;
	kbdTable[SDLK_f] = EC_F;
	kbdTable[SDLK_g] = EC_G;
	kbdTable[SDLK_h] = EC_H;
	kbdTable[SDLK_i] = EC_I;
	kbdTable[SDLK_j] = EC_J;
	kbdTable[SDLK_k] = EC_K;
	kbdTable[SDLK_l] = EC_L;
	kbdTable[SDLK_m] = EC_M;
	kbdTable[SDLK_n] = EC_N;
	kbdTable[SDLK_o] = EC_O;
	kbdTable[SDLK_p] = EC_P;
	kbdTable[SDLK_q] = EC_Q;
	kbdTable[SDLK_r] = EC_R;
	kbdTable[SDLK_s] = EC_S;
	kbdTable[SDLK_t] = EC_T;
	kbdTable[SDLK_u] = EC_U;
	kbdTable[SDLK_v] = EC_V;
	kbdTable[SDLK_w] = EC_W;
	kbdTable[SDLK_x] = EC_X;
	kbdTable[SDLK_y] = EC_Y;
	kbdTable[SDLK_z] = EC_Z;

	kbdTable[SDLK_F1       ] = EC_F1;
	kbdTable[SDLK_F2       ] = EC_F2;
	kbdTable[SDLK_F3       ] = EC_F3;
	kbdTable[SDLK_F4       ] = EC_F4;
	kbdTable[SDLK_F5       ] = EC_F5;
	kbdTable[SDLK_ESCAPE   ] = EC_ESC;
	kbdTable[SDLK_TAB      ] = EC_TAB;
	kbdTable[SDLK_PAGEUP   ] = EC_STOP;
	kbdTable[SDLK_BACKSPACE] = EC_BKSPACE;
	kbdTable[SDLK_END      ] = EC_SELECT;
	kbdTable[SDLK_RETURN   ] = EC_RETURN;
	kbdTable[SDLK_SPACE    ] = EC_SPACE;
	kbdTable[SDLK_HOME     ] = EC_CLS;
	kbdTable[SDLK_INSERT   ] = EC_INS;
	kbdTable[SDLK_DELETE   ] = EC_DEL;
	kbdTable[SDLK_LEFT     ] = EC_LEFT;
	kbdTable[SDLK_UP       ] = EC_UP;
	kbdTable[SDLK_RIGHT    ] = EC_RIGHT;
	kbdTable[SDLK_DOWN     ] = EC_DOWN;

	kbdTable[SDLK_KP_MULTIPLY] = EC_NUMMUL;
	kbdTable[SDLK_KP_PLUS    ] = EC_NUMADD;
	kbdTable[SDLK_KP_DIVIDE  ] = EC_NUMDIV;
	kbdTable[SDLK_KP_MINUS   ] = EC_NUMSUB;
	kbdTable[SDLK_KP_PERIOD  ] = EC_NUMPER;
	kbdTable[SDLK_PAGEDOWN   ] = EC_NUMCOM;
	kbdTable[SDLK_KP0] = EC_NUM0;
	kbdTable[SDLK_KP1] = EC_NUM1;
	kbdTable[SDLK_KP2] = EC_NUM2;
	kbdTable[SDLK_KP3] = EC_NUM3;
	kbdTable[SDLK_KP4] = EC_NUM4;
	kbdTable[SDLK_KP5] = EC_NUM5;
	kbdTable[SDLK_KP6] = EC_NUM6;
	kbdTable[SDLK_KP7] = EC_NUM7;
	kbdTable[SDLK_KP8] = EC_NUM8;
	kbdTable[SDLK_KP9] = EC_NUM9;

	kbdTable[SDLK_LSUPER  ] = EC_TORIKE;
	kbdTable[SDLK_RSUPER  ] = EC_JIKKOU;
	kbdTable[SDLK_LSHIFT  ] = EC_LSHIFT;
	kbdTable[SDLK_RSHIFT  ] = EC_RSHIFT;
	kbdTable[SDLK_LCTRL   ] = EC_CTRL;
	kbdTable[SDLK_LALT    ] = EC_GRAPH;
	kbdTable[SDLK_RALT    ] = EC_CODE;
	kbdTable[SDLK_CAPSLOCK] = EC_CAPS;
	kbdTable[SDLK_KP_ENTER] = EC_PAUSE;
	kbdTable[SDLK_SYSREQ  ] = EC_PRINT;
}

void keyboardInit()
{
	initKbdTable();
	inputEventReset();
}

void keyboardUpdate(SDL_KeyboardEvent *event)
{
	if (event->type == SDL_KEYUP) {
		inputEventUnset(kbdTable[event->keysym.sym]);
	} else if (event->type == SDL_KEYDOWN) {
		inputEventSet(kbdTable[event->keysym.sym]);
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
