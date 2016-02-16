/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_CPU_H
#define M_CPU_H

#include "util/common.h"

struct mCPUComponent {
	uint32_t id;
	void (*init)(void* cpu, struct mCPUComponent* component);
	void (*deinit)(struct mCPUComponent* component);
};

#endif
