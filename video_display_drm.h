/*
  Copyright (C) 2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#ifndef VIDEO_DISPLAY_DRM_H
#define VIDEO_DISPLAY_DRM_H

#include "video_display.h"

extern "C" {
#include "external/drmu/drmu/drmu_math.h"
}


//--------------------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------------------
typedef struct drmprime_out_env_s drmprime_out_env_t;
typedef struct drmprime_video_env_s drmprime_video_env_t;
typedef struct drmu_plane_s drmu_plane_t;
typedef struct drmu_dmabuf_env_s drmu_dmabuf_env_t;
typedef struct drmu_fb_s drmu_fb_t;


//--------------------------------------------------------------------------------------------------
// Video display class using DRM
//--------------------------------------------------------------------------------------------------
class CVideoDisplayDRM : public CVideoDisplay
{
public:
	CVideoDisplayDRM() { }
	virtual ~CVideoDisplayDRM();

	virtual bool BInit( SDL_Window *pWindow ) override;

	virtual SDL_Surface *InitOverlay( int nWidth, int nHeight ) override;
	virtual void SetOverlayRect( const SDL_Rect &rect ) override;
	virtual void UpdateOverlay() override;

	virtual bool BInitCodec( AVCodecContext *pContext, const AVCodec *pCodec ) override;
	virtual void SetVideoRect( const SDL_Rect &rect ) override;
	virtual void UpdateVideo( AVFrame *pFrame ) override;

	virtual void DisplayFrame() override;

private:
	drmprime_out_env_t *m_pDisplayOut = nullptr;
	drmprime_video_env_t *m_pVideoOut = nullptr;
	drmu_plane_t *m_pOverlayPlane = nullptr;
	drmu_dmabuf_env_t *m_pOverlayDMABufEnv = nullptr;
	drmu_fb_t *m_arrOverlayFB[2] = { nullptr, nullptr };
	int m_iOverlayFB = 0;
	SDL_Surface *m_pOverlaySurface = nullptr;
	drmu_rect_t m_OverlayRect = { 0, 0, 0, 0 };
	drmu_rect_t m_VideoRect = { 0, 0, 0, 0 };
};

#endif // VIDEO_DISPLAY_DRM_H
