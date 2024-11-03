/*
  Copyright (C) 2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#ifndef VIDEO_DISPLAY_WAYLAND_H
#define VIDEO_DISPLAY_WAYLAND_H

#include "video_display.h"

extern "C" {
#include "external/hello_wayland/init_window.h"
}


//--------------------------------------------------------------------------------------------------
// Video display class using Wayland
//--------------------------------------------------------------------------------------------------
class CVideoDisplayWayland : public CVideoDisplay
{
public:
	CVideoDisplayWayland() { }
	virtual ~CVideoDisplayWayland();

	virtual bool BInit( SDL_Window *pWindow ) override;

	virtual SDL_Surface *InitOverlay( int nWidth, int nHeight ) override;
	virtual void SetOverlayRect( const SDL_Rect &rect ) override;
	virtual void UpdateOverlay() override;

	virtual bool BInitCodec( AVCodecContext *pContext, const AVCodec *pCodec ) override;
	virtual void SetVideoRect( const SDL_Rect &rect ) override;
	virtual void UpdateVideo( AVFrame *pFrame ) override;

	virtual void DisplayFrame() override;

	void HandleEvent( const SDL_Event *pEvent );

private:
	SDL_WindowID m_unWindowID = 0;
	vid_out_env_t *m_pVideoOut = nullptr;
	wo_surface_t *m_pOverlayWaylandSurface = nullptr;
	wo_fb_t *m_pOverlayFB = nullptr;
	bool m_bOverlayAttached = false;
	SDL_Surface *m_pOverlaySurface;
	wo_rect_t m_OverlayRect = { 0, 0, 0, 0 };
};

#endif // VIDEO_DISPLAY_WAYLAND_H
