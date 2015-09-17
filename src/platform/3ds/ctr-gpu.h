/* Copyright (c) 2015 Yuri Kunde Schlesner
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GUI_GPU_H
#define GUI_GPU_H

#include <3ds.h>

struct ctrTexture {
	void* data;
	u32 format;
	u32 filter;
	u16 width;
	u16 height;
};

inline void ctrTexture_Init(struct ctrTexture* tex) {
	tex->data = NULL;
	tex->format = GPU_RGB565;
	tex->filter = GPU_NEAREST;
	tex->width = 0;
	tex->height = 0;
}

Result ctrInitGpu(void);
void ctrDeinitGpu(void);

void ctrGpuBeginFrame(void);
void ctrGpuEndFrame(void* outputFramebuffer, int w, int h);

void ctrSetViewportSize(s16 w, s16 h);

void ctrActivateTexture(const struct ctrTexture* texture);
void ctrAddRectScaled(u32 color, s16 x, s16 y, s16 w, s16 h, s16 u, s16 v, s16 uw, s16 vh);
void ctrAddRect(u32 color, s16 x, s16 y, s16 u, s16 v, s16 w, s16 h);
void ctrFlushBatch(void);

#endif
