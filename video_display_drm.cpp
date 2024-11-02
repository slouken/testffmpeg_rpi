/*
  Copyright (C) 2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include "video_display_drm.h"
#include "video_display_rpi.h"

#include <drm_fourcc.h>

#include "external/drmu/drmu/drmu.h"
#include "external/drmu/drmu/drmu_output.h"
#include "external/drmu/drmu/drmu_dmabuf.h"
extern "C" {
#include "external/drmu/test/drmprime_out.h"
}


//--------------------------------------------------------------------------------------------------
// Return DRM hardware buffer
//--------------------------------------------------------------------------------------------------
static int get_drm_buffer2( struct AVCodecContext *s, struct AVFrame *frame, int flags )
{
	drmprime_video_env_t * const dve = (drmprime_video_env_t * const)s->opaque;

	return drmprime_video_get_buffer2( dve, s, frame, flags );
}


//--------------------------------------------------------------------------------------------------
// CVideoDisplayDRM destructor
//--------------------------------------------------------------------------------------------------
CVideoDisplayDRM::~CVideoDisplayDRM()
{
	if ( m_pOverlaySurface )
	{
		SDL_DestroySurface( m_pOverlaySurface );
	}
	if ( m_pVideoOut )
	{
		drmprime_video_delete( m_pVideoOut );
	}
	if ( m_pDisplayOut )
	{
		if ( m_pOverlayPlane )
		{
			drmu_output_t *pOutput = drmprime_out_drmu_output( m_pDisplayOut );
			drmu_env_t *pOutputEnv = drmu_output_env( pOutput );
			drmu_atomic_t *pAtomic = drmu_atomic_new( pOutputEnv );
			drmu_atomic_plane_clear_add( pAtomic, m_pOverlayPlane );
			drmu_atomic_queue( &pAtomic);

			for ( int iIndex = 0; iIndex < SDL_arraysize( m_arrOverlayFB ); ++iIndex )
			{
				drmu_fb_unref( &m_arrOverlayFB[ iIndex ] );
			}
			drmu_dmabuf_env_unref( &m_pOverlayDMABufEnv );
			drmu_plane_unref( &m_pOverlayPlane );
		}
		drmprime_out_delete( m_pDisplayOut );
	}
}


//--------------------------------------------------------------------------------------------------
// Initialize the video display
//--------------------------------------------------------------------------------------------------
bool CVideoDisplayDRM::BInit( SDL_Window *pWindow )
{
	int nFD = (int)SDL_GetNumberProperty( SDL_GetWindowProperties( pWindow ), SDL_PROP_WINDOW_KMSDRM_DRM_FD_NUMBER, -1 );
	if ( nFD < 0 )
	{
		SDL_SetError( "Couldn't get DRM file descriptor" );
		return false;
	}

	m_pDisplayOut = drmprime_out_new_fd( nFD );
	if ( !m_pDisplayOut )
	{
		SDL_SetError( "Couldn't create display output" );
		return false;
	}

	m_pVideoOut = drmprime_video_new( m_pDisplayOut );
	if ( !m_pVideoOut )
	{
		SDL_SetError( "Couldn't create video output" );
		return false;
	}

	return true;
}


//--------------------------------------------------------------------------------------------------
// Initialize the video overlay
//--------------------------------------------------------------------------------------------------
SDL_Surface *CVideoDisplayDRM::InitOverlay( int nWidth, int nHeight )
{
	drmu_output_t *pOutput = drmprime_out_drmu_output( m_pDisplayOut );
	drmu_env_t *pOutputEnv = drmu_output_env( pOutput );

	m_pOverlayPlane = drmu_output_plane_ref_format( pOutput, DRMU_PLANE_TYPE_OVERLAY, DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_LINEAR );
	if ( !m_pOverlayPlane )
	{
		SDL_SetError( "Couldn't find overlay plane" );
		return nullptr;
	}

	m_pOverlayDMABufEnv = drmu_dmabuf_env_new_video( pOutputEnv );

	for ( int iIndex = 0; iIndex < SDL_arraysize( m_arrOverlayFB ); ++iIndex )
	{
		if ( m_pOverlayDMABufEnv )
		{
			m_arrOverlayFB[ iIndex ] = drmu_fb_new_dmabuf_mod( m_pOverlayDMABufEnv, nWidth, nHeight, DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_LINEAR );
		}
		else
		{
			m_arrOverlayFB[ iIndex ] = drmu_fb_new_dumb_mod( pOutputEnv, nWidth, nHeight, DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_LINEAR );
		}
		if ( !m_arrOverlayFB[ iIndex ] )
		{
			SDL_SetError( "Couldn't create overlay framebuffer" );
			return nullptr;
		}
	}

	m_pOverlaySurface = SDL_CreateSurface( nWidth, nHeight, SDL_PIXELFORMAT_ARGB8888 );
	if ( !m_pOverlaySurface )
	{
		return nullptr;
	}
	return m_pOverlaySurface;
}


//--------------------------------------------------------------------------------------------------
// Set the overlay display rect
//--------------------------------------------------------------------------------------------------
void CVideoDisplayDRM::SetOverlayRect( const SDL_Rect &rect )
{
	m_OverlayRect.x = rect.x;
	m_OverlayRect.y = rect.y;
	m_OverlayRect.w = (uint32_t)rect.w;
	m_OverlayRect.h = (uint32_t)rect.h;
}


//--------------------------------------------------------------------------------------------------
// Update the overlay with new content
//--------------------------------------------------------------------------------------------------
void CVideoDisplayDRM::UpdateOverlay()
{
	m_iOverlayFB = ( m_iOverlayFB + 1 ) % SDL_arraysize( m_arrOverlayFB );

	drmu_fb_t *pFB = m_arrOverlayFB[ m_iOverlayFB ];
	drmu_fb_write_start( pFB );
	const uint8_t *pSrc = (uint8_t *)m_pOverlaySurface->pixels;
	int nSrcPitch = m_pOverlaySurface->pitch;
	uint8_t *pDst = (uint8_t *)drmu_fb_data( pFB, 0 );
	int nDstPitch = drmu_fb_pitch( pFB, 0 );
	if ( nSrcPitch == nDstPitch )
	{
		memcpy( pDst, pSrc, m_pOverlaySurface->h * nSrcPitch );
	}
	else
	{
		size_t unLength = m_pOverlaySurface->w * 4;
		for ( int i = m_pOverlaySurface->h; i--; )
		{
			memcpy( pDst, pSrc, unLength );
			pSrc += nSrcPitch;
			pDst += nDstPitch;
		}
	}
	drmu_fb_write_end( pFB );

	drmu_output_t *pOutput = drmprime_out_drmu_output( m_pDisplayOut );
	drmu_env_t *pOutputEnv = drmu_output_env( pOutput );
	drmu_atomic_t *pAtomic = drmu_atomic_new( pOutputEnv );
	drmu_atomic_plane_clear_add( pAtomic, m_pOverlayPlane );
	drmu_atomic_plane_add_fb( pAtomic, m_pOverlayPlane, pFB, m_OverlayRect );
	drmu_atomic_queue( &pAtomic);
}


//--------------------------------------------------------------------------------------------------
// Initialize the video codec
//--------------------------------------------------------------------------------------------------
bool CVideoDisplayDRM::BInitCodec( AVCodecContext *pContext, const AVCodec *pCodec )
{
	return ::BInitCodec( pContext, pCodec, get_drm_buffer2, m_pVideoOut );
}


//--------------------------------------------------------------------------------------------------
// Set the video display rect
//--------------------------------------------------------------------------------------------------
void CVideoDisplayDRM::SetVideoRect( const SDL_Rect &rect )
{
	m_VideoRect.x = rect.x;
	m_VideoRect.y = rect.y;
	m_VideoRect.w = (uint32_t)rect.w;
	m_VideoRect.h = (uint32_t)rect.h;
}


//--------------------------------------------------------------------------------------------------
// Update the video frame being displayed
//--------------------------------------------------------------------------------------------------
void CVideoDisplayDRM::UpdateVideo( AVFrame *pFrame )
{
	drmprime_video_display( m_pVideoOut, pFrame );
}


//--------------------------------------------------------------------------------------------------
// Display the video frame and overlay
//--------------------------------------------------------------------------------------------------
void CVideoDisplayDRM::DisplayFrame()
{
}

