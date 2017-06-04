/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GB_CORE_H
#define GB_CORE_H

#include <mgba-util/common.h>

CXX_GUARD_START

struct mCore;
struct mCore* GBCoreCreate(void);
#ifndef MINIMAL_CORE
struct mCore* GBVideoLogPlayerCreate(void);
#endif

CXX_GUARD_END

#endif
