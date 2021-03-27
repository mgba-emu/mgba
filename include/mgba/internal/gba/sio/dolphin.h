/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SIO_DOLPHIN_H
#define SIO_DOLPHIN_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/timing.h>
#include <mgba/internal/gba/sio.h>

#include <mgba-util/socket.h>

extern const uint16_t DOLPHIN_CLOCK_PORT;
extern const uint16_t DOLPHIN_DATA_PORT;

struct GBASIODolphin {
	struct GBASIODriver d;
	struct mTimingEvent event;

	Socket data;
	Socket clock;

	int32_t clockSlice;
	int state;
	bool active;
};

void GBASIODolphinCreate(struct GBASIODolphin*);
void GBASIODolphinDestroy(struct GBASIODolphin*);

bool GBASIODolphinConnect(struct GBASIODolphin*, const struct Address* address, short dataPort, short clockPort);
bool GBASIODolphinIsConnected(struct GBASIODolphin*);

CXX_GUARD_END

#endif
