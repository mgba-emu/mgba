/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef UTIL_STRING_H
#define UTIL_STRING_H

#include <mgba-util/common.h>

CXX_GUARD_START

#ifndef HAVE_STRNDUP
// This is sometimes a macro
char* strndup(const char* start, size_t len);
#endif

#ifndef HAVE_STRDUP
char* strdup(const char* str);
#endif

#ifndef HAVE_STRLCPY
size_t strlcpy(char* restrict dst, const char* restrict src, size_t dstsize);
#endif

char* strnrstr(const char* restrict s1, const char* restrict s2, size_t len);
bool endswith(const char* restrict s1, const char* restrict end);
bool startswith(const char* restrict s1, const char* restrict start);

size_t toUtf8(uint32_t unichar, char* buffer);
size_t toUtf16(uint32_t unichar, uint16_t* buffer);
int utfcmp(const uint16_t* utf16, const char* utf8, size_t utf16Length, size_t utf8Length);
char* utf16to8(const uint16_t* utf16, size_t length);
uint32_t utf8Char(const char** unicode, size_t* length);
uint32_t utf16Char(const uint16_t** unicode, size_t* length);
char* latin1ToUtf8(const char* latin1, size_t length);
char* gbkToUtf8(const char* gbk, size_t length);
size_t utf8strlen(const char* string);

int hexDigit(char digit);
const char* hex32(const char* line, uint32_t* out);
const char* hex24(const char* line, uint32_t* out);
const char* hex16(const char* line, uint16_t* out);
const char* hex12(const char* line, uint16_t* out);
const char* hex8(const char* line, uint8_t* out);
const char* hex4(const char* line, uint8_t* out);

void rtrim(char* string);

ssize_t parseQuotedString(const char* unparsed, ssize_t unparsedLen, char* parsed, ssize_t parsedLen);
bool wildcard(const char* search, const char* string);

CXX_GUARD_END

#endif
