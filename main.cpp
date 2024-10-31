/*
  Copyright (C) 2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
}

#include "video_display.h"

#include "icon.h"

static SDL_Surface *sprite;
static SDL_Rect *positions;
static SDL_Rect *velocities;
static int num_sprites = 10;

static SDL_Window *window;
static SDL_Renderer *renderer;
static CVideoDisplay *display;
static SDL_Surface *overlay;
static int video_width;
static int video_height;
static SDL_AudioStream *audio;
static Uint64 video_start;
static bool verbose;

#undef av_err2str
static char av_error[512];
#define av_err2str(result) av_make_error_string(av_error, sizeof(av_error), result)

static SDL_Surface *CreateSprite(unsigned char *data, unsigned int len)
{
    SDL_Surface *surface;
    SDL_IOStream *src = SDL_IOFromConstMem(data, len);
    if (src) {
        surface = SDL_LoadBMP_IO(src, true);
        if (surface) {
            /* Treat white as transparent */
            SDL_SetSurfaceColorKey(surface, true, SDL_MapSurfaceRGB(surface, 255, 255, 255));
            SDL_SetSurfaceRLE(surface, true);
        }
    }
    return surface;
}

static void MoveSprite(void)
{
    SDL_Rect *position, *velocity;
    int i;

    /* Clear the overlay to transparent */
    SDL_FillSurfaceRect(overlay, NULL, 0);

    for (i = 0; i < num_sprites; ++i) {
        position = &positions[i];
        velocity = &velocities[i];
        position->x += velocity->x;
        if ((position->x < 0) || (position->x >= (overlay->w - sprite->w))) {
            velocity->x = -velocity->x;
            position->x += velocity->x;
        }
        position->y += velocity->y;
        if ((position->y < 0) || (position->y >= (overlay->h - sprite->h))) {
            velocity->y = -velocity->y;
            position->y += velocity->y;
        }
    }

    /* Blit the sprite onto the overlay */
    for (i = 0; i < num_sprites; ++i) {
        position = &positions[i];

        SDL_BlitSurface(sprite, NULL, overlay, position);
    }

    display->UpdateOverlay();
}

static void UpdateOverlayRect()
{
    int window_width = 0, window_height = 0;
    SDL_Rect rect = { 0, 0, 0, 0 };

    if (SDL_GetWindowSize(window, &window_width, &window_height)) {
        rect.w = window_width;
        rect.h = (window_width * overlay->h) / overlay->w;
        rect.y = window_height - rect.h;
    }
    display->SetOverlayRect(rect);
}

static void UpdateVideoRect()
{
    int window_width = 0, window_height = 0;
    SDL_Rect rect = { 0, 0, 0, 0 };

    if (SDL_GetWindowSize(window, &window_width, &window_height) && video_width && video_height) {
        if (video_width >= video_height) {
            rect.w = window_width;
            rect.h = (window_width * video_height) / video_width;
            rect.y = (window_height - rect.h) / 2;
        } else {
            rect.h = window_height;
            rect.w = (window_height * video_width) / video_height;
            rect.x = (window_width - rect.w) / 2;
        }
    }
    display->SetVideoRect(rect);
}

static AVCodecContext *OpenVideoStream(AVFormatContext *ic, int stream, const AVCodec *codec)
{
    AVStream *st = ic->streams[stream];
    AVCodecParameters *codecpar = st->codecpar;
    AVCodecContext *context;
    int result;

    SDL_Log("Video stream: %s %dx%d\n", avcodec_get_name(codec->id), codecpar->width, codecpar->height);

    context = avcodec_alloc_context3(NULL);
    if (!context) {
        SDL_Log("avcodec_alloc_context3 failed");
        return NULL;
    }

    result = avcodec_parameters_to_context(context, ic->streams[stream]->codecpar);
    if (result < 0) {
        SDL_Log("avcodec_parameters_to_context failed: %s\n", av_err2str(result));
        avcodec_free_context(&context);
        return NULL;
    }
    context->pkt_timebase = ic->streams[stream]->time_base;

    if (!display->BInitCodec(context, codec)) {
        SDL_Log("Couldn't initialize codec: %s\n", SDL_GetError());
        avcodec_free_context(&context);
        return NULL;
    }

    return context;
}

static void HandleVideoFrame(AVFrame *frame, double pts)
{
    int width = frame->width - (frame->crop_left + frame->crop_right);
    int height = frame->height - (frame->crop_top + frame->crop_bottom);
    if (width != video_width || height != video_height) {
        video_width = width;
        video_height = height;
        UpdateVideoRect();
    }

    display->UpdateVideo(frame);

    MoveSprite();

    /* Quick and dirty PTS handling */
    if (!video_start) {
        video_start = SDL_GetTicks();
    }
    double now = (double)(SDL_GetTicks() - video_start) / 1000.0;
    if (now < pts) {
        SDL_DelayPrecise((Uint64)((pts - now) * SDL_NS_PER_SECOND));
    }

    display->DisplayFrame();
}

static AVCodecContext *OpenAudioStream(AVFormatContext *ic, int stream, const AVCodec *codec)
{
    AVStream *st = ic->streams[stream];
    AVCodecParameters *codecpar = st->codecpar;
    AVCodecContext *context;
    int result;

    SDL_Log("Audio stream: %s %d channels, %d Hz\n", avcodec_get_name(codec->id), codecpar->ch_layout.nb_channels, codecpar->sample_rate);

    context = avcodec_alloc_context3(NULL);
    if (!context) {
        SDL_Log("avcodec_alloc_context3 failed\n");
        return NULL;
    }

    result = avcodec_parameters_to_context(context, ic->streams[stream]->codecpar);
    if (result < 0) {
        SDL_Log("avcodec_parameters_to_context failed: %s\n", av_err2str(result));
        avcodec_free_context(&context);
        return NULL;
    }
    context->pkt_timebase = ic->streams[stream]->time_base;

    result = avcodec_open2(context, codec, NULL);
    if (result < 0) {
        SDL_Log("Couldn't open codec %s: %s", avcodec_get_name(context->codec_id), av_err2str(result));
        avcodec_free_context(&context);
        return NULL;
    }

    SDL_AudioSpec spec = { SDL_AUDIO_F32, codecpar->ch_layout.nb_channels, codecpar->sample_rate };
    audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (audio) {
        SDL_ResumeAudioStreamDevice(audio);
    } else {
        SDL_Log("Couldn't open audio: %s", SDL_GetError());
    }
    return context;
}

static SDL_AudioFormat GetAudioFormat(int format)
{
    switch (format) {
    case AV_SAMPLE_FMT_U8:
    case AV_SAMPLE_FMT_U8P:
        return SDL_AUDIO_U8;
    case AV_SAMPLE_FMT_S16:
    case AV_SAMPLE_FMT_S16P:
        return SDL_AUDIO_S16;
    case AV_SAMPLE_FMT_S32:
    case AV_SAMPLE_FMT_S32P:
        return SDL_AUDIO_S32;
    case AV_SAMPLE_FMT_FLT:
    case AV_SAMPLE_FMT_FLTP:
        return SDL_AUDIO_F32;
    default:
        /* Unsupported */
        return SDL_AUDIO_UNKNOWN;
    }
}

static bool IsPlanarAudioFormat(int format)
{
    switch (format) {
    case AV_SAMPLE_FMT_U8P:
    case AV_SAMPLE_FMT_S16P:
    case AV_SAMPLE_FMT_S32P:
    case AV_SAMPLE_FMT_FLTP:
    case AV_SAMPLE_FMT_DBLP:
    case AV_SAMPLE_FMT_S64P:
        return true;
    default:
        return false;
    }
}

static void InterleaveAudio(AVFrame *frame, const SDL_AudioSpec *spec)
{
    int c, n;
    int samplesize = SDL_AUDIO_BYTESIZE(spec->format);
    int framesize = SDL_AUDIO_FRAMESIZE(*spec);
    Uint8 *data = (Uint8 *)SDL_malloc(frame->nb_samples * framesize);
    if (!data) {
        return;
    }

    /* This could be optimized with SIMD and not allocating memory each time */
    for (c = 0; c < spec->channels; ++c) {
        const Uint8 *src = frame->data[c];
        Uint8 *dst = data + c * samplesize;
        for (n = frame->nb_samples; n--;) {
            SDL_memcpy(dst, src, samplesize);
            src += samplesize;
            dst += framesize;
        }
    }
    SDL_PutAudioStreamData(audio, data, frame->nb_samples * framesize);
    SDL_free(data);
}

static void HandleAudioFrame(AVFrame *frame)
{
    if (audio) {
        SDL_AudioSpec spec = { GetAudioFormat(frame->format), frame->ch_layout.nb_channels, frame->sample_rate };
        SDL_SetAudioStreamFormat(audio, &spec, NULL);

        if (frame->ch_layout.nb_channels > 1 && IsPlanarAudioFormat(frame->format)) {
            InterleaveAudio(frame, &spec);
        } else {
            SDL_PutAudioStreamData(audio, frame->data[0], frame->nb_samples * SDL_AUDIO_FRAMESIZE(spec));
        }
    }
}

static void av_log_callback(void *avcl, int level, const char *fmt, va_list vl)
{
    const char *pszCategory = NULL;
    char *message;

    switch (level) {
    case AV_LOG_PANIC:
    case AV_LOG_FATAL:
        pszCategory = "fatal error";
        break;
    case AV_LOG_ERROR:
        pszCategory = "error";
        break;
    case AV_LOG_WARNING:
        pszCategory = "warning";
        break;
    case AV_LOG_INFO:
        pszCategory = "info";
        break;
    case AV_LOG_VERBOSE:
        pszCategory = "verbose";
        break;
    case AV_LOG_DEBUG:
        if (verbose) {
            pszCategory = "debug";
        }
        break;
    }

    if (!pszCategory) {
        // We don't care about this message
        return;
    }

    SDL_vasprintf(&message, fmt, vl);
    SDL_Log("ffmpeg %s: %s", pszCategory, message);
    SDL_free(message);
}

static void print_usage(const char *argv0)
{
    SDL_Log("Usage: %s [--verbose] [--video wayland|x11|kmsdrm] [--fullscreen] video_file\n", argv0);
}


int main(int argc, char *argv[])
{
    const char *file = NULL;
    AVFormatContext *ic = NULL;
    int audio_stream = -1;
    int video_stream = -1;
    const AVCodec *audio_codec = NULL;
    const AVCodec *video_codec = NULL;
    AVCodecContext *audio_context = NULL;
    AVCodecContext *video_context = NULL;
    AVPacket *pkt = NULL;
    AVFrame *frame = NULL;
    double first_pts = -1.0;
    int i;
    int result;
    int return_code = -1;
    SDL_WindowFlags window_flags = (SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
    int window_width = 1280;
    int window_height = 720;
    bool flushing = false;
    bool decoded = false;
    bool done = false;

    /* Log ffmpeg messages */
    av_log_set_callback(av_log_callback);

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed = 0;

        if (SDL_strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
            consumed = 1;
        } else if (SDL_strcmp(argv[i], "--video") == 0 && argv[i + 1]) {
            SDL_SetHint(SDL_HINT_VIDEO_DRIVER, argv[i + 1]);
            consumed = 2;
        } else if (SDL_strcmp(argv[i], "--geometry") == 0 && argv[i + 1]) {
            if (SDL_sscanf(argv[i + 1], "%dx%d", &window_width, &window_height) == 2) {
                consumed = 2;
            }
        } else if (SDL_strcmp(argv[i], "--fullscreen") == 0) {
            window_flags |= SDL_WINDOW_FULLSCREEN;
            consumed = 1;
        } else if (!file) {
            /* We'll try to open this as a media file */
            file = argv[i];
            consumed = 1;
        }
        if (!consumed) {
            print_usage(argv[0]);
            return_code = 1;
            goto quit;
        }

        i += consumed;
    }

    if (!file) {
        print_usage(argv[0]);
        return_code = 1;
        goto quit;
    }

    if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s\n", SDL_GetError());
        return_code = 2;
        goto quit;
    }

    window = SDL_CreateWindow(file, window_width, window_height, window_flags);
    if (!window) {
        SDL_Log("Couldn't create window: %s\n", SDL_GetError());
        return_code = 2;
        goto quit;
    }

    display = CreateVideoDisplay(window);
    if (!display) {
        SDL_Log("Couldn't create video display: %s\n", SDL_GetError());
        return_code = 3;
        goto quit;
    }

    overlay = display->InitOverlay( 1280, 256 );
    if (!overlay) {
        SDL_Log("Couldn't create video overlay: %s\n", SDL_GetError());
        return_code = 3;
        goto quit;
    }
    UpdateOverlayRect();

    renderer = SDL_CreateSoftwareRenderer(overlay);
    if (!renderer) {
        SDL_Log("Couldn't create overlay renderer: %s\n", SDL_GetError());
        return_code = 3;
        goto quit;
    }

    /* Open the media file */
    result = avformat_open_input(&ic, file, NULL, NULL);
    if (result < 0) {
        SDL_Log("Couldn't open %s: %d", argv[1], result);
        return_code = 4;
        goto quit;
    }
    video_stream = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, -1, -1, &video_codec, 0);
    if (video_stream >= 0) {
        video_context = OpenVideoStream(ic, video_stream, video_codec);
        if (!video_context) {
            return_code = 4;
            goto quit;
        }
    }
    audio_stream = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, -1, video_stream, &audio_codec, 0);
    if (audio_stream >= 0) {
        audio_context = OpenAudioStream(ic, audio_stream, audio_codec);
        if (!audio_context) {
            return_code = 4;
            goto quit;
        }
    }
    pkt = av_packet_alloc();
    if (!pkt) {
        SDL_Log("av_packet_alloc failed");
        return_code = 4;
        goto quit;
    }
    frame = av_frame_alloc();
    if (!frame) {
        SDL_Log("av_frame_alloc failed");
        return_code = 4;
        goto quit;
    }

    /* Allocate memory for the sprite info */
    positions = (SDL_Rect *)SDL_malloc(num_sprites * sizeof(*positions));
    velocities = (SDL_Rect *)SDL_malloc(num_sprites * sizeof(*velocities));
    if (!positions || !velocities) {
        SDL_Log("Out of memory!\n");
        return_code = 3;
        goto quit;
    }

    /* Create the sprite */
    sprite = CreateSprite(icon_bmp, icon_bmp_len);
    if (!sprite) {
        SDL_Log("Couldn't create sprite: %s", SDL_GetError());
        return_code = 3;
        goto quit;
    }

    /* Position sprites and set their velocities */
    SDL_Rect viewport;
    SDL_GetRenderViewport(renderer, &viewport);
    for (i = 0; i < num_sprites; ++i) {
        positions[i].x = SDL_rand(viewport.w - sprite->w);
        positions[i].y = SDL_rand(viewport.h - sprite->h);
        positions[i].w = sprite->w;
        positions[i].h = sprite->h;
        velocities[i].x = 0;
        velocities[i].y = 0;
        while (velocities[i].x == 0 || velocities[i].y == 0) {
            velocities[i].x = (SDL_rand(2 + 1) - 1);
            velocities[i].y = (SDL_rand(2 + 1) - 1);
        }
    }

    /* We're ready to go! */
    SDL_ShowWindow(window);

    /* Main render loop */
    while (!done) {
        SDL_Event event;

        /* Check for events */
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_WINDOW_RESIZED:
                UpdateOverlayRect();
                UpdateVideoRect();
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE) {
                    done = true;
                }
                break;
            case SDL_EVENT_QUIT:
                done = true;
                break;
            }
        }

        if (!flushing) {
            result = av_read_frame(ic, pkt);
            if (result < 0) {
                SDL_Log("End of stream, finishing decode\n");
                if (video_context) {
                    avcodec_flush_buffers(video_context);
                }
                flushing = true;
            } else {
                if (pkt->stream_index == audio_stream) {
                    result = avcodec_send_packet(audio_context, pkt);
                    if (result < 0) {
                        SDL_Log("avcodec_send_packet(audio_context) failed: %s", av_err2str(result));
                    }
                } else if (pkt->stream_index == video_stream) {
                    result = avcodec_send_packet(video_context, pkt);
                    if (result < 0) {
                        SDL_Log("avcodec_send_packet(video_context) failed: %s", av_err2str(result));
                    }
                }
                av_packet_unref(pkt);
            }
        }

        decoded = false;
        if (audio_context) {
            while (avcodec_receive_frame(audio_context, frame) >= 0) {
                HandleAudioFrame(frame);
                decoded = true;
            }
            if (flushing) {
                /* Let SDL know we're done sending audio */
                SDL_FlushAudioStream(audio);
            }
        }
        if (video_context) {
            while (avcodec_receive_frame(video_context, frame) >= 0) {
                double pts = ((double)frame->pts * video_context->pkt_timebase.num) / video_context->pkt_timebase.den;
                if (first_pts < 0.0) {
                    first_pts = pts;
                }
                pts -= first_pts;

                HandleVideoFrame(frame, pts);
                decoded = true;
            }
        }

        if (flushing && !decoded) {
            if (SDL_GetAudioStreamQueued(audio) > 0) {
                /* Wait a little bit for the audio to finish */
                SDL_Delay(10);
            } else {
                done = true;
            }
        }
    }
    return_code = 0;

quit:
    SDL_free(positions);
    SDL_free(velocities);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&audio_context);
    avcodec_free_context(&video_context);
    avformat_close_input(&ic);
    if (display) {
        delete display;
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    return return_code;
}
