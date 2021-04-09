/* Copyright (c) 2013-2020 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef FFMPEG_COMMON
#define FFMPEG_COMMON

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/interface.h>

#include <libavformat/avformat.h>
#include <libavcodec/version.h>

// Version 57.16 in FFmpeg
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 37, 100)
#define FFMPEG_USE_PACKETS
#endif

// Version 57.15 in libav
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 35, 0)
#define FFMPEG_USE_NEW_BSF
#endif

// Version 57.14 in libav
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 0)
#define FFMPEG_USE_CODECPAR
#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 8, 0)
#define FFMPEG_USE_PACKET_UNREF
#endif

static inline enum AVPixelFormat mColorFormatToFFmpegPixFmt(enum mColorFormat format) {
	switch (format) {
#ifndef USE_LIBAV
	case mCOLOR_XRGB8:
		return AV_PIX_FMT_0RGB;
	case mCOLOR_XBGR8:
		return AV_PIX_FMT_0BGR;
	case mCOLOR_RGBX8:
		return AV_PIX_FMT_RGB0;
	case mCOLOR_BGRX8:
		return AV_PIX_FMT_BGR0;
#else
	case mCOLOR_XRGB8:
		return AV_PIX_FMT_ARGB;
	case mCOLOR_XBGR8:
		return AV_PIX_FMT_ABGR;
	case mCOLOR_RGBX8:
		return AV_PIX_FMT_RGBA;
	case mCOLOR_BGRX8:
		return AV_PIX_FMT_BGRA;
#endif
	case mCOLOR_ARGB8:
		return AV_PIX_FMT_ARGB;
	case mCOLOR_ABGR8:
		return AV_PIX_FMT_ABGR;
	case mCOLOR_RGBA8:
		return AV_PIX_FMT_RGBA;
	case mCOLOR_BGRA8:
		return AV_PIX_FMT_BGRA;
	case mCOLOR_RGB5:
		return AV_PIX_FMT_RGB555;
	case mCOLOR_BGR5:
		return AV_PIX_FMT_BGR555;
	case mCOLOR_RGB565:
		return AV_PIX_FMT_RGB565;
	case mCOLOR_BGR565:
		return AV_PIX_FMT_BGR565;
	case mCOLOR_RGB8:
		return AV_PIX_FMT_RGB24;
	case mCOLOR_BGR8:
		return AV_PIX_FMT_BGR24;
	case mCOLOR_L8:
		return AV_PIX_FMT_GRAY8;
	default:
		return AV_PIX_FMT_NONE;
	}
}

CXX_GUARD_END

#endif
