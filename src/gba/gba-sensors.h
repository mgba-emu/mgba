/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_SENSORS_H
#define GBA_SENSORS_H

#include "util/common.h"

struct GBARotationSource {
	void (*sample)(struct GBARotationSource*);

	int32_t (*readTiltX)(struct GBARotationSource*);
	int32_t (*readTiltY)(struct GBARotationSource*);

	int32_t (*readGyroZ)(struct GBARotationSource*);
};

#endif
