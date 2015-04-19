/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SIO_DOLPHIN_H
#define SIO_DOLPHIN_H

#include "gba/sio.h"

#include "util/socket.h"

extern const uint16_t DOLPHIN_CLOCK_PORT;
extern const uint16_t DOLPHIN_DATA_PORT;

struct GBASIODolphin {
	struct GBASIODriver d;

	Socket data;
	Socket clock;

	int32_t nextEvent;
	int32_t clockSlice;
};

void GBASIODolphinCreate(struct GBASIODolphin*);
void GBASIODolphinDestroy(struct GBASIODolphin*);

bool GBASIODolphinConnect(struct GBASIODolphin*, const struct Address* address, short dataPort, short clockPort);

#endif
