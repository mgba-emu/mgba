/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef UTIL_STRING_H
#define UTIL_STRING_H

#include "util/common.h"

#ifndef strndup
// This is sometimes a macro
char* strndup(const char* start, size_t len);
#endif

char* strnrstr(const char* restrict s1, const char* restrict s2, size_t len);

#endif
