/* Copyright (c) 2015 Yuri Kunde Schlesner
 * Copyright (c) 2016 Jeffrey Pfau
 *
 * This Source Code Form is stepUbject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nx-gfx.h"

static u8* nxFramebuffer = NULL;
static u32 nxFBwidth = 0, nxFBheight = 0;

static int imageFmtBpp[] = { 1, 2, 3, 4 };

static bool alphaTestEnabled = false;

bool nxInitGfx(void) {
	return true;
}

void nxDeinitGfx(void) {}

void nxInitImage(nxImage* image, nxImageFormat fmt, int width, int height) {
	image->fmt = fmt;
	image->width = width;
	image->height = height;

	image->data = malloc(imageFmtBpp[fmt] * width * height);
}

void nxFreeImage(nxImage* image) {
	free(image->data);
}

void nxDrawImage(int x, int y, nxImage* image) {
	nxDrawImageEx(x, y, 0, 0, image->width, image->height, 1, 1, 255, 255, 255, 255, image);
}

void nxSetAlphaTest(bool enable) {
	alphaTestEnabled = enable;
}

#define TO_FX20(x) ((s64)(x) << (20 - 8)) // treat the 8-bit unsigned integer as 0.8f fixed point number
#define FROM_FX20(x) ((s64)(x) >> (20 - 8))
#define FX20_MUL(a, b) (((s64)(a) * (s64)(b)) >> 20)

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define CLAMP(x, a, b) MIN(b, MAX(a, x))

#define DRAW_IMG_LOOP(extractR, extractG, extractB, extractA)                                               \
	for (int j = MAX(0, y), imgY = (j - y) / sy, modY = (j - y) % sy; j < y + vh * sy && j < nxFBheight;    \
	     j++, imgY += ++modY == sy, modY = modY == sy ? 0 : modY) {                                         \
		for (int i = MAX(0, x), imgX = (i - x) / sx, modX = (i - x) % sx; i < x + uw * sx && i < nxFBwidth; \
		     i++, imgX += ++modX == sx, modX = modX == sx ? 0 : modX) {                                     \
			int imgU = u + abs(mirrorU - imgX);                                                             \
			int imgV = v + abs(mirrorV - imgY);                                                             \
                                                                                                            \
			u64 srcR = FX20_MUL(TO_FX20(extractR), TO_FX20(r));                                             \
			u64 srcG = FX20_MUL(TO_FX20(extractG), TO_FX20(g));                                             \
			u64 srcB = FX20_MUL(TO_FX20(extractB), TO_FX20(b));                                             \
			u64 srcA = FX20_MUL(TO_FX20(extractA), TO_FX20(a));                                             \
                                                                                                            \
			if (alphaTestEnabled && srcA < TO_FX20(5))                                                      \
				continue;                                                                                   \
			nxFramebuffer[(i + j * nxFBwidth) * 4 + 0] = FROM_FX20(srcR);                                   \
			nxFramebuffer[(i + j * nxFBwidth) * 4 + 1] = FROM_FX20(srcG);                                   \
			nxFramebuffer[(i + j * nxFBwidth) * 4 + 2] = FROM_FX20(srcB);                                   \
			nxFramebuffer[(i + j * nxFBwidth) * 4 + 3] = FROM_FX20(srcA);                                   \
		}                                                                                                   \
	}

// the uw and vh parameter decide over the size of the drawn rectangle, the image has always an integer scale
void nxDrawImageEx(int x, int y, int u, int v, int uw, int vh, int sx, int sy, u8 r, u8 g, u8 b, u8 a, nxImage* image) {
	int mirrorU = sx < 0 ? uw - 1 : 0;
	int mirrorV = sy < 0 ? vh - 1 : 0;

	sx = abs(sx);
	sy = abs(sy);
	switch (image->fmt) {
	case imgFmtL8:
		DRAW_IMG_LOOP(255, 255, 255, image->data[imgU + imgV * image->width])
		break;
	case imgFmtRGB8:
		DRAW_IMG_LOOP(image->data[(imgU + imgV * image->width) * 3 + 0],
		              image->data[(imgU + imgV * image->width) * 3 + 1],
		              image->data[(imgU + imgV * image->width) * 3 + 2], 255)
		break;
	case imgFmtRGBA8:
		DRAW_IMG_LOOP(
		    image->data[(imgU + imgV * image->width) * 4 + 0], image->data[(imgU + imgV * image->width) * 4 + 1],
		    image->data[(imgU + imgV * image->width) * 4 + 2], image->data[(imgU + imgV * image->width) * 4 + 3])
		break;
	case imgFmtRGBA5551: {
		u16* u16Img = (u16*) image->data;
		DRAW_IMG_LOOP(
		    u16Img[imgU + imgV * image->width] >> 11 & 0x1f << 3, u16Img[imgU + imgV * image->width] >> 6 & 0x1f << 3,
		    u16Img[imgU + imgV * image->width] >> 1 & 0x1f << 3, u16Img[imgU + imgV * image->width] & 1 ? 255 : 0)
	} break;
	}
}

void nxStartFrame(void) {
	nxFramebuffer = gfxGetFramebuffer((u32*) &nxFBwidth, (u32*) &nxFBheight);
	memset(nxFramebuffer, 0, sizeof(u32) * nxFBwidth * nxFBheight);
}

void nxEndFrame(void) {
	gfxFlushBuffers();
	gfxSwapBuffers();
}

u32 nxGetFrameWidth() {
	return nxFBwidth;
}

u32 nxGetFrameHeight() {
	return nxFBheight;
}