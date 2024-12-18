/*
  Copyright (C) 2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include "video_display.h"
#include "video_display_drm.h"
#include "video_display_egl.h"
#include "video_display_wayland.h"


CVideoDisplay *CreateVideoDisplay( SDL_Window *pWindow )
{
	CVideoDisplay *pDisplay;

	if ( SDL_strcmp( SDL_GetCurrentVideoDriver(), "kmsdrm" ) == 0 )
	{
		pDisplay = new CVideoDisplayDRM;
	}
	else if ( SDL_strcmp( SDL_GetCurrentVideoDriver(), "wayland" ) == 0 )
	{
		pDisplay = new CVideoDisplayWayland;
	}
	else
	{
		pDisplay = new CVideoDisplayEGL;
	}

	if ( !pDisplay->BInit( pWindow ) )
	{
		delete pDisplay;
		return nullptr;
	}
	return pDisplay;
}
