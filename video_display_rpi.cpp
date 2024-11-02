/*
  Copyright (C) 2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include "video_display_rpi.h"

#include <SDL3/SDL.h>

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_drm.h>
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
// Initialize the video codec
//--------------------------------------------------------------------------------------------------
bool BInitCodec( AVCodecContext *pContext, const AVCodec *pCodec, int (*get_buffer2)( AVCodecContext *s, AVFrame *frame, int flags ), void *pOpaque )
{
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
		pContext->get_buffer2 = get_buffer2;
		pContext->opaque = pOpaque;

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

