/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_GBP_H
#define GBA_GBP_H

#include <mgba-util/common.h>

CXX_GUARD_START

struct GBASIOPlayer;
struct GBASIOPlayerKeyCallback {
	struct mKeyCallback d;
	struct GBASIOPlayer* p;
};

struct GBASIOPlayer {
	struct GBASIODriver d;
	struct GBA* p;
	unsigned inputsPosted;
	int txPosition;
	struct mTimingEvent event;
	struct GBASIOPlayerKeyCallback callback;
	bool oldOpposingDirections;
	struct mKeyCallback* oldCallback;
};

void GBASIOPlayerInit(struct GBASIOPlayer* gbp);
void GBASIOPlayerReset(struct GBASIOPlayer* gbp);

struct GBAVideo;
void GBASIOPlayerUpdate(struct GBA* gba);
bool GBASIOPlayerCheckScreen(const struct GBAVideo* video);

CXX_GUARD_END

#endif
