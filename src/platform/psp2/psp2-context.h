/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef PSP2_CONTEXT_H
#define PSP2_CONTEXT_H

#include "psp2-common.h"
#include <mgba/core/interface.h>
#include <mgba-util/gui.h>

struct mGUIRunner;
void mPSP2Setup(struct mGUIRunner* runner);
void mPSP2Teardown(struct mGUIRunner* runner);
void mPSP2MapKey(struct mInputMap* map, int pspKey, int key);

void mPSP2LoadROM(struct mGUIRunner* runner);
void mPSP2UnloadROM(struct mGUIRunner* runner);
void mPSP2PrepareForFrame(struct mGUIRunner* runner);
void mPSP2Paused(struct mGUIRunner* runner);
void mPSP2Unpaused(struct mGUIRunner* runner);
void mPSP2Swap(struct mGUIRunner* runner);
void mPSP2Draw(struct mGUIRunner* runner, bool faded);
void mPSP2DrawScreenshot(struct mGUIRunner* runner, const color_t* pixels, unsigned width, unsigned height, bool faded);
void mPSP2IncrementScreenMode(struct mGUIRunner* runner);
void mPSP2SetFrameLimiter(struct mGUIRunner* runner, bool limit);
uint16_t mPSP2PollInput(struct mGUIRunner* runner);
bool mPSP2SystemPoll(struct mGUIRunner* runner);

#endif
