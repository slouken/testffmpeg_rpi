/*
  Copyright (C) 2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#ifndef VIDEO_DISPLAY_EGL_H
#define VIDEO_DISPLAY_EGL_H

#include "video_display.h"


//--------------------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------------------
typedef struct vid_out_env_s vid_out_env_t;


//--------------------------------------------------------------------------------------------------
// Video display class using EGL
//--------------------------------------------------------------------------------------------------
class CVideoDisplayEGL : public CVideoDisplay
{
public:
	CVideoDisplayEGL() { }
	virtual ~CVideoDisplayEGL();

	virtual bool BInit( SDL_Window *pWindow ) override;

	virtual EDisplayType GetDisplayType() override { return k_EDisplayTypeEGL; }

	virtual SDL_Surface *InitOverlay( int nWidth, int nHeight ) override;
	virtual void SetOverlayRect( const SDL_Rect &rect ) override;
	virtual void UpdateOverlay() override;

	virtual bool BInitCodec( AVCodecContext *pContext, const AVCodec *pCodec ) override;
	virtual void SetVideoRect( const SDL_Rect &rect ) override;
	virtual void UpdateVideo( AVFrame *pFrame ) override;

	virtual void DisplayFrame() override;

private:
	SDL_Renderer *m_pRenderer = nullptr;
	vid_out_env_t *m_pVideoOut = nullptr;
	SDL_Surface *m_pOverlaySurface = nullptr;
	SDL_Texture *m_pOverlayTexture = nullptr;
	SDL_FRect m_OverlayRect = { 0.0f, 0.0f, 0.0f, 0.0f };
	SDL_Texture *m_pVideoTexture = nullptr;
	SDL_FRect m_VideoRect = { 0.0f, 0.0f, 0.0f, 0.0f };
};

#endif // VIDEO_DISPLAY_EGL_H
