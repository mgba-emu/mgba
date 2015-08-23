/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef UTIL_STRING_H
#define UTIL_STRING_H

#include "util/common.h"

#ifndef HAVE_STRNDUP
// This is sometimes a macro
char* strndup(const char* start, size_t len);
#endif

#ifndef HAVE_STRDUP
char* strdup(const char* str);
#endif

char* strnrstr(const char* restrict s1, const char* restrict s2, size_t len);

int utfcmp(const uint16_t* utf16, const char* utf8, size_t utf16Length, size_t utf8Length);
char* utf16to8(const uint16_t* utf16, size_t length);

int hexDigit(char digit);
const char* hex32(const char* line, uint32_t* out);
const char* hex16(const char* line, uint16_t* out);

#endif
