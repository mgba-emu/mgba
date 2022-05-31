/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef HASH_H
#define HASH_H

#include <mgba-util/common.h>

CXX_GUARD_START

uint32_t hash32(const void* key, size_t len, uint32_t seed);

CXX_GUARD_END

#endif
