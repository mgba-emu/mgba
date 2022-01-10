/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EVENTPUMP_H
#define EVENTPUMP_H

#include <mgba-util/common.h>

CXX_GUARD_START

void processEvents(void);
void postEvent(void (^block)());
void setupPostEvent(void);

CXX_GUARD_END

#endif
