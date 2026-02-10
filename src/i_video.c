
// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//	DOOM graphics stuff for SDL2.
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#endif

#include <SDL.h>

#include "doomstat.h"
#include "i_system.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"

#include "doomdef.h"

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static SDL_Color palette[256];

// Blocky mode,
// replace each 320x200 pixel with multiply*multiply pixels.
// According to Dave Taylor, it still is a bonehead thing
// to use ....
static int	multiply=1;

//
//  Translates the key currently in X_event
//

int xlatekey(SDL_Keysym *sym)
{

    int rc;

    switch(sym->sym)
    {
      case SDLK_LEFT:	rc = KEY_LEFTARROW;	break;
      case SDLK_RIGHT:	rc = KEY_RIGHTARROW;	break;
      case SDLK_DOWN:	rc = KEY_DOWNARROW;	break;
      case SDLK_UP:	rc = KEY_UPARROW;	break;
      case SDLK_a:	rc = KEY_LEFTARROW;	break;
      case SDLK_d:	rc = KEY_RIGHTARROW;	break;
      case SDLK_s:	rc = KEY_DOWNARROW;	break;
      case SDLK_w:	rc = KEY_UPARROW;	break;
      case SDLK_ESCAPE:	rc = KEY_ESCAPE;	break;
      case SDLK_RETURN:	rc = KEY_ENTER;		break;
      case SDLK_TAB:	rc = KEY_TAB;		break;
      case SDLK_F1:	rc = KEY_F1;		break;
      case SDLK_F2:	rc = KEY_F2;		break;
      case SDLK_F3:	rc = KEY_F3;		break;
      case SDLK_F4:	rc = KEY_F4;		break;
      case SDLK_F5:	rc = KEY_F5;		break;
      case SDLK_F6:	rc = KEY_F6;		break;
      case SDLK_F7:	rc = KEY_F7;		break;
      case SDLK_F8:	rc = KEY_F8;		break;
      case SDLK_F9:	rc = KEY_F9;		break;
      case SDLK_F10:	rc = KEY_F10;		break;
      case SDLK_F11:	rc = KEY_F11;		break;
      case SDLK_F12:	rc = KEY_F12;		break;
	
      case SDLK_BACKSPACE:
      case SDLK_DELETE:	rc = KEY_BACKSPACE;	break;

      case SDLK_PAUSE:	rc = KEY_PAUSE;		break;

      case SDLK_EQUALS:	rc = KEY_EQUALS;	break;

      case SDLK_MINUS:	rc = KEY_MINUS;		break;

      case SDLK_LSHIFT:
      case SDLK_RSHIFT:
	rc = KEY_RSHIFT;
	break;
	
      case SDLK_LCTRL:
      case SDLK_RCTRL:
	rc = KEY_RCTRL;
	break;
	
      case SDLK_LALT:
      case SDLK_LGUI:
      case SDLK_RALT:
      case SDLK_RGUI:
	rc = KEY_RALT;
	break;
	
      default:
	rc = sym->sym;
	break;
    }

    return rc;

}

void I_ShutdownGraphics(void)
{
  if (texture) SDL_DestroyTexture(texture);
  if (renderer) SDL_DestroyRenderer(renderer);
  if (window) SDL_DestroyWindow(window);
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
}



//
// I_StartFrame
//
void I_StartFrame (void)
{
    // er?

}

static int	lastmousex = 0;
static int	lastmousey = 0;
boolean		mousemoved = false;

void I_GetEvent(void)
{

    SDL_Event sdlevent;
    event_t event;

    while (SDL_PollEvent(&sdlevent))
    {
        switch (sdlevent.type)
        {
            case SDL_QUIT:
                I_Quit();
                break;
            case SDL_KEYDOWN:
                event.type = ev_keydown;
                event.data1 = xlatekey(&sdlevent.key.keysym);
                D_PostEvent(&event);
                break;
            case SDL_KEYUP:
                event.type = ev_keyup;
                event.data1 = xlatekey(&sdlevent.key.keysym);
                D_PostEvent(&event);
                break;
        }
    }

}


//
// I_StartTic
//
void I_StartTic (void)
{
    I_GetEvent();
}


//
// I_UpdateNoBlit
//
void I_UpdateNoBlit (void)
{
    // what is this?
}

//
// I_FinishUpdate
//
void I_FinishUpdate (void)
{

    static int	lasttic;
    int		tics;
    int		i;

    // draws little dots on the bottom of the screen
    if (devparm)
    {

	i = I_GetTime();
	tics = i - lasttic;
	lasttic = i;
	if (tics > 20) tics = 20;

	for (i=0 ; i<tics*2 ; i+=2)
	    screens[0][ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0xff;
	for ( ; i<20*2 ; i+=2)
	    screens[0][ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0x0;
    
    }

    void *pixels;
    int pitch;
    SDL_LockTexture(texture, NULL, &pixels, &pitch);
    
    uint32_t *dst = (uint32_t *)pixels;
    byte *src = screens[0];
    
    for (int j = 0; j < SCREENWIDTH * SCREENHEIGHT; j++)
    {
        byte val = src[j];
        SDL_Color col = palette[val];
        dst[j] = (0xFF << 24) | (col.r << 16) | (col.g << 8) | col.b;
    }
    
    SDL_UnlockTexture(texture);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);

}


//
// I_ReadScreen
//
void I_ReadScreen (byte* scr)
{
    memcpy (scr, screens[0], SCREENWIDTH*SCREENHEIGHT);
}


//
// I_SetPalette
//
void I_SetPalette (byte* pal)
{
    for (int i = 0; i < 256; i++)
    {
        palette[i].r = gammatable[usegamma][*pal++];
        palette[i].g = gammatable[usegamma][*pal++];
        palette[i].b = gammatable[usegamma][*pal++];
        palette[i].a = 255;
    }
}


void I_InitGraphics(void)
{

    static int		firsttime=1;

    if (!firsttime)
	return;
    firsttime = 0;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        I_Error("SDL_Init(SDL_INIT_VIDEO) failed: %s", SDL_GetError());
    }

    if (M_CheckParm("-2"))
	multiply = 2;

    if (M_CheckParm("-3"))
	multiply = 3;

    if (M_CheckParm("-4"))
	multiply = 4;

    if (multiply == 1)
        multiply = 4;

    int width = SCREENWIDTH * multiply;
    int height = SCREENHEIGHT * multiply;

    window = SDL_CreateWindow("DoomMetal",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                              width, height, 
                              SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
                              
    if (!window) {
        I_Error("SDL_CreateWindow failed: %s", SDL_GetError());
    }

    renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) {
        I_Error("SDL_CreateRenderer failed: %s", SDL_GetError());
    }
    
    SDL_RenderSetLogicalSize(renderer, SCREENWIDTH, SCREENHEIGHT);

    texture = SDL_CreateTexture(renderer, 
                                SDL_PIXELFORMAT_ARGB8888, 
                                SDL_TEXTUREACCESS_STREAMING, 
                                SCREENWIDTH, SCREENHEIGHT);

    if (!texture) {
         I_Error("SDL_CreateTexture failed: %s", SDL_GetError());
    }
}
