/* Copyright (c) 2015 Yuri Kunde Schlesner
 * Copyright (c) 2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GUI_GPU_H
#define GUI_GPU_H

#include <3ds.h>
#include <citro3d.h>

bool ctrInitGpu(void);
void ctrDeinitGpu(void);

void ctrSetViewportSize(s16 w, s16 h, bool tilt);

void ctrActivateTexture(const C3D_Tex* texture);
void ctrTextureMultiply(void);
void ctrTextureBias(u32 color);
void ctrAddRectEx(u32 color, s16 x, s16 y, s16 w, s16 h, s16 u, s16 v, s16 uw, s16 vh, float rotate);
void ctrAddRect(u32 color, s16 x, s16 y, s16 u, s16 v, s16 w, s16 h);
void ctrFlushBatch(void);
void ctrStartFrame(void);
void ctrEndFrame(void);

#endif
