/*
  Copyright (C) 2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#ifndef VIDEO_DISPLAY_H
#define VIDEO_DISPLAY_H

#include <SDL3/SDL.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

//--------------------------------------------------------------------------------------------------
// A video display class
//--------------------------------------------------------------------------------------------------
class CVideoDisplay
{
public:
	CVideoDisplay() { }
	virtual ~CVideoDisplay() { }

	virtual bool BInit( SDL_Window *pWindow ) = 0;

	virtual SDL_Surface *InitOverlay( int nWidth, int nHeight ) = 0;
	virtual void SetOverlayRect( const SDL_Rect &rect ) = 0;
	virtual void UpdateOverlay() = 0;

	virtual bool BInitCodec( AVCodecContext *pContext, const AVCodec *pCodec ) = 0;
	virtual void SetVideoRect( const SDL_Rect &rect ) = 0;
	virtual void UpdateVideo( AVFrame *pFrame ) = 0;

	virtual void DisplayFrame() = 0;
};


//--------------------------------------------------------------------------------------------------
// Create a video display instance for an SDL window
//--------------------------------------------------------------------------------------------------
extern CVideoDisplay *CreateVideoDisplay( SDL_Window *pWindow );

#endif // VIDEO_DISPLAY_H
