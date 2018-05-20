/* Copyright (c) 2015 Yuri Kunde Schlesner
 * Copyright (c) 2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GUI_GPU_H
#define GUI_GPU_H

#include <switch.h>

typedef enum { imgFmtL8, imgFmtRGBA5551, imgFmtRGB8, imgFmtRGBA8 } nxImageFormat;

typedef struct {
	nxImageFormat fmt;
	u16 width, height;
	u8* data;
} nxImage;

void nxInitImage(nxImage* image, nxImageFormat fmt, int width, int height);
void nxFreeImage(nxImage* image);

bool nxInitGfx(void);
void nxDeinitGfx(void);

void nxSetAlphaTest(bool enable);

void nxDrawImage(int x, int y, nxImage* image);
void nxDrawImageEx(int x, int y, int u, int v, int uw, int vh, int sx, int sy, u8 r, u8 g, u8 b, u8 a, nxImage* image);

void nxStartFrame(void);
void nxEndFrame(void);

u32 nxGetFrameWidth();
u32 nxGetFrameHeight();

#endif
