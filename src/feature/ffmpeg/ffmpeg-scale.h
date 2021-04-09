/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef FFMPEG_SCALE
#define FFMPEG_SCALE

#include <mgba-util/common.h>

CXX_GUARD_START

#include "feature/ffmpeg/ffmpeg-common.h"

void FFmpegScale(const void* input, int iwidth, int iheight, unsigned istride,
                 void* output, int owidth, int oheight, unsigned ostride,
                 enum mColorFormat format, int quality);

CXX_GUARD_END

#endif
