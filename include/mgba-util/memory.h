/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef MEMORY_H
#define MEMORY_H

#include <mgba-util/common.h>

CXX_GUARD_START

void* anonymousMemoryMap(size_t size);
void mappedMemoryFree(void* memory, size_t size);

CXX_GUARD_END

#endif
