/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef FORMATTING_H
#define FORMATTING_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include "locale.h"

#ifdef HAVE_XLOCALE
#include <xlocale.h>
#elif !defined(HAVE_LOCALE)
typedef const char* locale_t;
#endif

int ftostr_l(char* restrict str, size_t size, float f, locale_t locale);

#ifndef HAVE_STRTOF_L
float strtof_l(const char* restrict str, char** restrict end, locale_t locale);
#endif

int ftostr_u(char* restrict str, size_t size, float f);
float strtof_u(const char* restrict str, char** restrict end);

#ifndef HAVE_LOCALTIME_R
struct tm* localtime_r(const time_t* timep, struct tm* result);
#endif

CXX_GUARD_END

#endif
