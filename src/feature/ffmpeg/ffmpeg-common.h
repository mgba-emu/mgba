/* Copyright (c) 2013-2020 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef FFMPEG_COMMON
#define FFMPEG_COMMON

#include <mgba-util/common.h>

CXX_GUARD_START

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

CXX_GUARD_END

#endif
