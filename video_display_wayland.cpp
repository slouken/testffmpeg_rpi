/*
  Copyright (C) 2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include "video_display_wayland.h"
#include "video_display_rpi.h"

#include <drm_fourcc.h>

//--------------------------------------------------------------------------------------------------
// CVideoDisplayWayland event watcher
//--------------------------------------------------------------------------------------------------
static bool EventWatch( void *pUserData, SDL_Event *pEvent )
{
	CVideoDisplayWayland *pDisplay = (CVideoDisplayWayland *)pUserData;
	pDisplay->HandleEvent( pEvent );
	return true;
}


//--------------------------------------------------------------------------------------------------
// CVideoDisplayWayland destructor
//--------------------------------------------------------------------------------------------------
CVideoDisplayWayland::~CVideoDisplayWayland()
{
	SDL_RemoveEventWatch( EventWatch, this );

	wo_fb_unref( &m_pOverlayFB );
	wo_surface_unref( &m_pOverlayWaylandSurface );
	if ( m_pOverlaySurface )
	{
		SDL_DestroySurface( m_pOverlaySurface );
	}
	if ( m_pVideoOut )
	{
		vidout_wayland_delete( m_pVideoOut );
	}
}


//--------------------------------------------------------------------------------------------------
// Initialize the video display
//--------------------------------------------------------------------------------------------------
bool CVideoDisplayWayland::BInit( SDL_Window *pWindow )
{
	m_unWindowID = SDL_GetWindowID( pWindow );

	SDL_PropertiesID unProps = SDL_GetWindowProperties( pWindow );
	wl_display *pDisplay = (wl_display *)SDL_GetPointerProperty( unProps, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL );
	if ( !pDisplay )
	{
		SDL_SetError( "Couldn't get Wayland display from window" );
		return false;
	}

	wl_surface *pSurface = (wl_surface *)SDL_GetPointerProperty( unProps, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL );
	if ( !pSurface )
	{
		SDL_SetError( "Couldn't get Wayland surface from window" );
		return false;
	}

	wp_viewport *pViewport = (wp_viewport *)SDL_GetPointerProperty( unProps, SDL_PROP_WINDOW_WAYLAND_VIEWPORT_POINTER, NULL );
	if ( !pViewport )
	{
		SDL_SetError( "Couldn't get Wayland viewport from window" );
		return false;
	}

	// Make sure the window size/fullscreen state changes have occurred
	SDL_SyncWindow( pWindow );
	int nWindowWidth = 0, nWindowHeight = 0;
	SDL_GetWindowSize( pWindow, &nWindowWidth, &nWindowHeight );

	wo_rect_t size = { 0, 0, (uint32_t)nWindowWidth, (uint32_t)nWindowHeight };
	m_pVideoOut = vidout_wayland_new_from( pDisplay, pSurface, pViewport, size );
	if ( !m_pVideoOut )
	{
		SDL_SetError( "Couldn't create video output" );
		return false;
	}

	SDL_AddEventWatch( EventWatch, this );

	return true;
}


//--------------------------------------------------------------------------------------------------
// Handle window resize events
//--------------------------------------------------------------------------------------------------
void CVideoDisplayWayland::HandleEvent( const SDL_Event *pEvent )
{
	if ( pEvent->type == SDL_EVENT_WINDOW_RESIZED && pEvent->window.windowID == m_unWindowID )
	{
		wo_window_t *pWindow = vidout_wayland_get_window( m_pVideoOut );
		wo_rect_t size = { 0, 0, (uint32_t)pEvent->window.data1, (uint32_t)pEvent->window.data2 };
		wo_window_set_size( pWindow, size );
	}
}


//--------------------------------------------------------------------------------------------------
// Initialize the video overlay
//--------------------------------------------------------------------------------------------------
SDL_Surface *CVideoDisplayWayland::InitOverlay( int nWidth, int nHeight )
{
	wo_window_t *pWindow = vidout_wayland_get_window( m_pVideoOut );
	wo_env_t *pWindowEnv = wo_window_env( pWindow );

	m_pOverlayWaylandSurface = wo_make_surface_z( pWindow, NULL, 20 );
	if ( !m_pOverlayWaylandSurface )
	{
		SDL_SetError( "Couldn't create overlay surface" );
		return nullptr;
	}

	m_pOverlayFB = wo_make_fb( pWindowEnv, nWidth, nHeight, DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_LINEAR );
	if ( !m_pOverlayFB )
	{
		SDL_SetError( "Couldn't create overlay framebuffer" );
		return nullptr;
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
void CVideoDisplayWayland::SetOverlayRect( const SDL_Rect &rect )
{
	if (rect.x == m_OverlayRect.x &&
	    rect.y == m_OverlayRect.y &&
	    rect.w == (int)m_OverlayRect.w &&
	    rect.h == (int)m_OverlayRect.h) {
		return;
	}

	m_OverlayRect.x = rect.x;
	m_OverlayRect.y = rect.y;
	m_OverlayRect.w = (uint32_t)rect.w;
	m_OverlayRect.h = (uint32_t)rect.h;

	if ( m_bOverlayAttached )
	{
		wo_surface_dst_pos_set( m_pOverlayWaylandSurface, m_OverlayRect );
	}
	else
	{
printf("attached at %d,%d %dx%d\n", m_OverlayRect.x, m_OverlayRect.y, m_OverlayRect.w, m_OverlayRect.h);
		wo_surface_attach_fb( m_pOverlayWaylandSurface, m_pOverlayFB, m_OverlayRect );
		m_bOverlayAttached = true;
	}
}


//--------------------------------------------------------------------------------------------------
// Update the overlay with new content
//--------------------------------------------------------------------------------------------------
void CVideoDisplayWayland::UpdateOverlay()
{
#define VERIFY_OVERLAY
#ifdef VERIFY_OVERLAY
static int i; ++i;
SDL_Rect rect = { 1, 1, 1, 1 };
SDL_FillSurfaceRect(m_pOverlaySurface, &rect, SDL_MapSurfaceRGBA(m_pOverlaySurface, i, 0, 0, 128));
#endif
	wo_fb_write_start( m_pOverlayFB );
	const uint8_t *pSrc = (uint8_t *)m_pOverlaySurface->pixels;
	int nSrcPitch = m_pOverlaySurface->pitch;
	uint8_t *pDst = (uint8_t *)wo_fb_data( m_pOverlayFB, 0 );
	int nDstPitch = (int)wo_fb_pitch( m_pOverlayFB, 0 );
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
	wo_fb_write_end( m_pOverlayFB );
}


//--------------------------------------------------------------------------------------------------
// Initialize the video codec
//--------------------------------------------------------------------------------------------------
bool CVideoDisplayWayland::BInitCodec( AVCodecContext *pContext, const AVCodec *pCodec )
{
	return ::BInitCodec( pContext, pCodec, vidout_wayland_get_buffer2, m_pVideoOut );
}


//--------------------------------------------------------------------------------------------------
// Update the video frame being displayed
//--------------------------------------------------------------------------------------------------
void CVideoDisplayWayland::UpdateVideo( AVFrame *pFrame )
{
	vidout_wayland_display( m_pVideoOut, pFrame );
}


//--------------------------------------------------------------------------------------------------
// Set the video display rect
//--------------------------------------------------------------------------------------------------
void CVideoDisplayWayland::SetVideoRect( const SDL_Rect &rect )
{
}


//--------------------------------------------------------------------------------------------------
// Display the video frame and overlay
//--------------------------------------------------------------------------------------------------
void CVideoDisplayWayland::DisplayFrame()
{
}

