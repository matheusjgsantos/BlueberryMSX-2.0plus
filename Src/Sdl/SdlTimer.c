/*****************************************************************************
** $Source: /cygdrive/d/Private/_SVNROOT/bluemsx/blueMSX/Src/Sdl/SdlTimer.c,v $
**
** $Revision: 1.6 $
**
** $Date: 2008-03-30 18:38:45 $
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
#include "ArchTimer.h"
#include <stdlib.h>
#include <SDL.h>

#ifdef NO_TIMERS

// Timer callbacks are not needed. When disabled, there is no need for
// archEvents either.

void* archCreateTimer(int period, int (*timerCallback)(void*)) { return NULL; }
void archTimerDestroy(void* timer) {}
static SDL_TimerID *my_timer_id 

// The only timer that is required is a high res timer. The resolution is
// not super important, the higher the better, but one tick every 10ms is
// good enough. The frequency argument is in Hz and is 1000 or less.
UInt32 archGetSystemUpTime(UInt32 frequency) 
{
    return SDL_GetTicks() / (1000 / frequency);
}

// This is just a timer value with a frequency as high as possible. 
// The frequency of the timer is not important.
UInt32 archGetHiresTimer() {
    return SDL_GetTicks();
}

#else 

static void (*timerCb)(void*) = NULL;
static UInt32 timerFreq;
static UInt32 lastTimeout;

Uint32 timerCalback(Uint32 interval)
{
    //fprintf(stderr,"SdlTimer is executing timerCallBack\n");
    //fprintf(stderr,"SdlTimer is about to get currentTime \n");
    //fprintf(stderr,"SdlTimer is about to get currentTime %d\n",archGetSystemUpTime(timerFreq));
    if (timerCb) {
        UInt32 currentTime = archGetSystemUpTime(timerFreq);
    	//fprintf(stderr,"SdlTimer got currentTime %d\n",currentTime);

        while (lastTimeout != currentTime) {
            lastTimeout = currentTime;
            timerCb(timerCb);
        }
    }
    return interval;
}

void* archCreateTimer(int period, int (*timerCallback)(void*)) 
{ 
    //fprintf(stderr,"SdlTimer is executing archCreateTimer\n");
    timerFreq = 1000 / period;
    lastTimeout = archGetSystemUpTime(timerFreq);
    timerCb  = timerCallback;

    // Deprecated on sdl2 -- SDL_SetTimer(period, timerCalback);
    SDL_TimerID my_timer_id =  SDL_AddTimer(period, timerCalback, NULL);
    //fprintf(stderr,"SDLTimer created a timer with id %d\n",my_timer_id);
    //fprintf(stderr,"SDLTimer will return timerCallback: %d\n",timerCallback);

    return timerCallback;
}

void archTimerDestroy(void* timer) 
{
    //fprintf(stderr,"SdlTimer is executing archTimerDestroy\n");
    if (timerCb != timer) {
        return;
    }

    //DEPRECATED in sdl2 -- SDL_SetTimer(0, NULL);
    //SDL_TimerID my_timer_id =  SDL_AddTimer(0, NULL, NULL);
    SDL_RemoveTimer(1);
    timerCb = NULL;
    //fprintf(stderr,"archTimerDestroy call ended\n");
}

UInt32 archGetSystemUpTime(UInt32 frequency) 
{
    //fprintf(stderr,"SdlTImer is about to return SDL_GetTicks() %d\n",SDL_GetTicks() / (1000 / frequency));
    return SDL_GetTicks() / (1000 / frequency);
}

UInt32 archGetHiresTimer() {
    return SDL_GetTicks();
}

#endif
