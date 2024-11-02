/*
  Copyright (C) 2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#ifndef VIDEO_DISPLAY_RPI_H
#define VIDEO_DISPLAY_RPI_H

extern "C" {
#include <libavcodec/avcodec.h>
}

//--------------------------------------------------------------------------------------------------
// Common functions for the Raspberry Pi video output
//--------------------------------------------------------------------------------------------------
bool BInitCodec( AVCodecContext *pContext, const AVCodec *pCodec, int (*get_buffer2)( AVCodecContext *s, AVFrame *frame, int flags ), void *pOpaque );

#endif // VIDEO_DISPLAY_RPI_H
