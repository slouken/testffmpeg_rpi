/*
  Copyright (C) 2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include "video_display_egl.h"

extern "C" {
#include <epoxy/gl.h>
#include <epoxy/egl.h>

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_drm.h>

#include "external/hello_wayland/init_window.h"
}


//--------------------------------------------------------------------------------------------------
// Return DRM pixel format
//--------------------------------------------------------------------------------------------------
static AVPixelFormat get_drm_format( AVCodecContext *ctx, const AVPixelFormat *pix_fmts )
{
	const AVPixelFormat *pPixelFormat;

	for ( pPixelFormat = pix_fmts; *pPixelFormat != AV_PIX_FMT_NONE; ++pPixelFormat )
	{
		AVPixelFormat eFormat = *pPixelFormat;

		if ( eFormat == AV_PIX_FMT_DRM_PRIME )
		{
			return eFormat;
		}
	}
	return AV_PIX_FMT_NONE;
}


//--------------------------------------------------------------------------------------------------
// CVideoDisplayEGL destructor
//--------------------------------------------------------------------------------------------------
CVideoDisplayEGL::~CVideoDisplayEGL()
{
	if ( m_pOverlaySurface )
	{
		SDL_DestroySurface( m_pOverlaySurface );
	}
	if ( m_pOverlayTexture )
	{
		SDL_DestroyTexture( m_pOverlayTexture );
	}
	if ( m_pVideoTexture )
	{
		SDL_DestroyTexture( m_pVideoTexture );
	}
	if ( m_pVideoOut )
	{
		vidout_wayland_delete( m_pVideoOut );
	}
	if ( m_pRenderer )
	{
		SDL_DestroyRenderer( m_pRenderer );
	}
}


//--------------------------------------------------------------------------------------------------
// Initialize the video display
//--------------------------------------------------------------------------------------------------
bool CVideoDisplayEGL::BInit( SDL_Window *pWindow )
{
	// Make sure we use EGL so we can import DMA-BUF images into textures
	SDL_SetHint( SDL_HINT_VIDEO_FORCE_EGL, "1" );

	m_pRenderer = SDL_CreateRenderer( pWindow, "opengles2" );
	if ( !m_pRenderer )
	{
		return false;
	}
	SDL_SetRenderVSync( m_pRenderer, 0 );

	return true;
}


//--------------------------------------------------------------------------------------------------
// Initialize the video overlay
//--------------------------------------------------------------------------------------------------
SDL_Surface *CVideoDisplayEGL::InitOverlay( int nWidth, int nHeight )
{
	m_pOverlayTexture = SDL_CreateTexture( m_pRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, nWidth, nHeight );
	if ( !m_pOverlayTexture )
	{
		return nullptr;
	}

	m_pOverlaySurface = SDL_CreateSurface( nWidth, nHeight, SDL_PIXELFORMAT_RGBA32 );
	if ( !m_pOverlaySurface )
	{
		return nullptr;
	}
	return m_pOverlaySurface;
}


//--------------------------------------------------------------------------------------------------
// Set the overlay display rect
//--------------------------------------------------------------------------------------------------
void CVideoDisplayEGL::SetOverlayRect( const SDL_Rect &rect )
{
	m_OverlayRect.x = (float)rect.x;
	m_OverlayRect.y = (float)rect.y;
	m_OverlayRect.w = (float)rect.w;
	m_OverlayRect.h = (float)rect.h;
}


//--------------------------------------------------------------------------------------------------
// Update the overlay with new content
//--------------------------------------------------------------------------------------------------
void CVideoDisplayEGL::UpdateOverlay()
{
	SDL_UpdateTexture( m_pOverlayTexture, nullptr, m_pOverlaySurface->pixels, m_pOverlaySurface->pitch );
}


//--------------------------------------------------------------------------------------------------
// Initialize the video codec
//--------------------------------------------------------------------------------------------------
bool CVideoDisplayEGL::BInitCodec( AVCodecContext *pContext, const AVCodec *pCodec )
{
	m_pVideoOut = vidout_simple_new();
	if ( !m_pVideoOut )
	{
		SDL_SetError( "Couldn't create video output" );
		return false;
	}

	if ( pCodec->id == AV_CODEC_ID_H264 )
	{
		// See if hardware decoding is available
		const AVCodec *pV4L2Codec = avcodec_find_decoder_by_name( "h264_v4l2m2m" );
		if ( pV4L2Codec )
		{
			pCodec = pV4L2Codec;
		}
	}

	bool bAccelerated = false;
	int iHWConfig = 0;
	const AVCodecHWConfig *pHWConfig;
	while ( ( pHWConfig = avcodec_get_hw_config( pCodec, iHWConfig++ ) ) != nullptr )
	{
		if ( pHWConfig->pix_fmt == AV_PIX_FMT_DRM_PRIME )
		{
			bAccelerated = true;
			break;
		}
	}

	if ( bAccelerated )
	{
		pContext->get_format = get_drm_format;

		if ( av_hwdevice_ctx_create( &pContext->hw_device_ctx, AV_HWDEVICE_TYPE_DRM, NULL, NULL, 0 ) < 0 )
		{
			SDL_SetError( "av_hwdevice_ctx_create() failed" );
			return false;
		}
	}
	else
	{
		pContext->get_buffer2 = vidout_wayland_get_buffer2;
		pContext->opaque = m_pVideoOut;

		// Allow ffmpeg to pick the number of threads
		pContext->thread_count = 0;
		pContext->thread_type = FF_THREAD_SLICE;
	}

#if LIBAVCODEC_VERSION_MAJOR < 60
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
	pContext->thread_safe_callbacks = 1;
#pragma GCC diagnostic pop
#endif

	if ( avcodec_open2( pContext, pCodec, nullptr ) < 0 )
	{
		SDL_SetError( "avcodec_open2() failed" );
		return false;
	}

	return true;
}


//--------------------------------------------------------------------------------------------------
// Set the video display rect
//--------------------------------------------------------------------------------------------------
void CVideoDisplayEGL::SetVideoRect( const SDL_Rect &rect )
{
	m_VideoRect.x = (float)rect.x;
	m_VideoRect.y = (float)rect.y;
	m_VideoRect.w = (float)rect.w;
	m_VideoRect.h = (float)rect.h;
}


//--------------------------------------------------------------------------------------------------
// Update the video frame being displayed
//--------------------------------------------------------------------------------------------------
void CVideoDisplayEGL::UpdateVideo( AVFrame *pFrame )
{
	int nWidth = pFrame->width - (pFrame->crop_left + pFrame->crop_right);
	int nHeight = pFrame->height - (pFrame->crop_top + pFrame->crop_bottom);

	// Free the previous texture
	SDL_DestroyTexture( m_pVideoTexture );

	m_pVideoTexture = SDL_CreateTexture( m_pRenderer, SDL_PIXELFORMAT_EXTERNAL_OES, SDL_TEXTUREACCESS_STATIC, nWidth, nHeight );
	if ( !m_pVideoTexture )
	{
		SDL_Log( "Couldn't create video texture: %s\n", SDL_GetError() );
		return;
	}

	EGLDisplay pDisplay = eglGetCurrentDisplay();
	const AVDRMFrameDescriptor *desc = get_frame_drm_descriptor( pFrame );
	EGLint attribs[50];
	EGLint *a = attribs;
	int i, j;
	GLuint texture;
	EGLImage image;

	static const EGLint anames[] = {
		EGL_DMA_BUF_PLANE0_FD_EXT,
		EGL_DMA_BUF_PLANE0_OFFSET_EXT,
		EGL_DMA_BUF_PLANE0_PITCH_EXT,
		EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT,
		EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT,
		EGL_DMA_BUF_PLANE1_FD_EXT,
		EGL_DMA_BUF_PLANE1_OFFSET_EXT,
		EGL_DMA_BUF_PLANE1_PITCH_EXT,
		EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT,
		EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT,
		EGL_DMA_BUF_PLANE2_FD_EXT,
		EGL_DMA_BUF_PLANE2_OFFSET_EXT,
		EGL_DMA_BUF_PLANE2_PITCH_EXT,
		EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT,
		EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT,
	};
	const EGLint *b = anames;

	*a++ = EGL_WIDTH;
	*a++ = nWidth;
	*a++ = EGL_HEIGHT;
	*a++ = nHeight;
	*a++ = EGL_LINUX_DRM_FOURCC_EXT;
	*a++ = desc->layers[0].format;

	for ( i = 0; i < desc->nb_layers; ++i ) {
		for ( j = 0; j < desc->layers[i].nb_planes; ++j ) {
			const AVDRMPlaneDescriptor *const p = desc->layers[i].planes + j;
			const AVDRMObjectDescriptor *const obj = desc->objects + p->object_index;
			*a++ = *b++;
			*a++ = obj->fd;
			*a++ = *b++;
			*a++ = p->offset;
			*a++ = *b++;
			*a++ = p->pitch;
			if ( obj->format_modifier == 0 ) {
				b += 2;
			} else {
				*a++ = *b++;
				*a++ = (EGLint)( obj->format_modifier & 0xFFFFFFFF );
				*a++ = *b++;
				*a++ = (EGLint)( obj->format_modifier >> 32 );
			}
		}
	}
	*a = EGL_NONE;

	if ( !( image = eglCreateImageKHR( pDisplay, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attribs ) ) ) {
		SDL_SetError( "Failed to import fd %d", desc->objects[0].fd );
		return;
	}

	// This binds the image to the texture we just created
	glEGLImageTargetTexture2DOES( GL_TEXTURE_EXTERNAL_OES, image );
	eglDestroyImageKHR( pDisplay, image );

	// A fence is set on the fd by the egl render - we can reuse the buffer once it goes away
	// ( same as the direct wayland output after buffer release )
	add_frame_fence( m_pVideoOut, pFrame );

}


//--------------------------------------------------------------------------------------------------
// Display the video frame and overlay
//--------------------------------------------------------------------------------------------------
void CVideoDisplayEGL::DisplayFrame()
{
	if ( !m_pVideoTexture || m_VideoRect.x || m_VideoRect.y )
	{
		SDL_SetRenderDrawColor( m_pRenderer, 0, 0, 0, 255 );
		SDL_RenderClear( m_pRenderer );
	}
	else
	{
		// The video will cover the whole window
	}

	SDL_RenderTexture( m_pRenderer, m_pVideoTexture, nullptr, &m_VideoRect );
	SDL_RenderTexture( m_pRenderer, m_pOverlayTexture, nullptr, &m_OverlayRect );
	SDL_RenderPresent( m_pRenderer );
}

