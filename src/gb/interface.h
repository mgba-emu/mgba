/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GB_INTERFACE_H
#define GB_INTERFACE_H

#include "util/common.h"

enum GBModel {
	GB_MODEL_DMG = 0x00,
	GB_MODEL_SGB = 0x40,
	GB_MODEL_CGB = 0x80,
	GB_MODEL_AGB = 0xC0
};

#endif
