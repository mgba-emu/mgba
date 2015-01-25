/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/string.h"

#include <string.h>

#ifndef HAVE_STRNDUP
char* strndup(const char* start, size_t len) {
	// This is suboptimal, but anything recent should have strndup
	char* out = malloc((len + 1) * sizeof(char));
	strncpy(out, start, len);
	out[len] = '\0';
	return out;
}
#endif

char* strnrstr(const char* restrict haystack, const char* restrict needle, size_t len) {
	char* last = 0;
	const char* next = haystack;
	size_t needleLen = strlen(needle);
	for (; len >= needleLen; --len, ++next) {
		if (strncmp(needle, next, needleLen) == 0) {
			last = (char*) next;
		}
	}
	return last;
}

static uint32_t _utf16Char(const uint16_t** unicode, size_t* length) {
	if (*length < 2) {
		*length = 0;
		return 0;
	}
	uint32_t unichar = **unicode;
	++*unicode;
	*length -= 2;
	if (unichar < 0xD800 || unichar >= 0xE000) {
		return unichar;
	}
	if (*length < 2) {
		*length = 0;
		return 0;
	}
	uint16_t highSurrogate = unichar;
	uint16_t lowSurrogate = **unicode;
	++*unicode;
	*length -= 2;
	if (highSurrogate >= 0xDC00) {
		return 0;
	}
	if (lowSurrogate < 0xDC00 || lowSurrogate >= 0xE000) {
		return 0;
	}
	highSurrogate -= 0xD800;
	lowSurrogate -= 0xDC00;
	return (highSurrogate << 10) + lowSurrogate + 0x10000;
}

static uint32_t _utf8Char(const char** unicode, size_t* length) {
	if (*length == 0) {
		return 0;
	}
	char byte = **unicode;
	--*length;
	++*unicode;
	if (!(byte & 0x80)) {
		return byte;
	}
	uint32_t unichar;
	static int tops[4] = { 0xC0, 0xE0, 0xF0, 0xF8 };
	size_t numBytes;
	for (numBytes = 0; numBytes < 3; ++numBytes) {
		if ((byte & tops[numBytes + 1]) == tops[numBytes]) {
			break;
		}
	}
	unichar = byte & ~tops[numBytes];
	if (numBytes == 3) {
		return 0;
	}
	++numBytes;
	if (*length < numBytes) {
		*length = 0;
		return 0;
	}
	size_t i;
	for (i = 0; i < numBytes; ++i) {
		unichar <<= 6;
		byte = **unicode;
		--*length;
		++*unicode;
		if ((byte & 0xC0) != 0x80) {
			return 0;
		}
		unichar |= byte & 0x3F;
	}
	return unichar;
}

static size_t _toUtf8(uint32_t unichar, char* buffer) {
	if (unichar > 0x10FFFF) {
		unichar = 0xFFFD;
	}
	if (unichar < 0x80) {
		buffer[0] = unichar;
		return 1;
	}
	if (unichar < 0x800) {
		buffer[0] = (unichar >> 6) | 0xC0;
		buffer[1] = (unichar & 0x3F) | 0x80;
		return 2;
	}
	if (unichar < 0x10000) {
		buffer[0] = (unichar >> 12) | 0xE0;
		buffer[1] = ((unichar >> 6) & 0x3F) | 0x80;
		buffer[2] = (unichar & 0x3F) | 0x80;
		return 3;
	}
	if (unichar < 0x200000) {
		buffer[0] = (unichar >> 18) | 0xF0;
		buffer[1] = ((unichar >> 12) & 0x3F) | 0x80;
		buffer[2] = ((unichar >> 6) & 0x3F) | 0x80;
		buffer[3] = (unichar & 0x3F) | 0x80;
		return 4;
	}

	// This shouldn't be possible
	return 0;
}

int utfcmp(const uint16_t* utf16, const char* utf8, size_t utf16Length, size_t utf8Length) {
	uint32_t char1 = 0, char2 = 0;
	while (utf16Length > 0 && utf8Length > 0) {
		if (char1 < char2) {
			return -1;
		}
		if (char1 > char2) {
			return 1;
		}
		char1 = _utf16Char(&utf16, &utf16Length);
		char2 = _utf8Char(&utf8, &utf8Length);
	}
	if (utf16Length == 0 && utf8Length > 0) {
		return -1;
	}
	if (utf16Length > 0 && utf8Length == 0) {
		return 1;
	}
	return 0;
}

char* utf16to8(const uint16_t* utf16, size_t length) {
	char* utf8 = 0;
	char* offset;
	char buffer[4];
	size_t utf8TotalBytes = 0;
	size_t utf8Length = 0;
	while (true) {
		if (length == 0) {
			break;
		}
		uint32_t unichar = _utf16Char(&utf16, &length);
		size_t bytes = _toUtf8(unichar, buffer);
		utf8Length += bytes;
		if (utf8Length < utf8TotalBytes) {
			memcpy(offset, buffer, bytes);
			offset += bytes;
		} else if (!utf8) {
			utf8 = malloc(length);
			if (!utf8) {
				return 0;
			}
			utf8TotalBytes = length;
			memcpy(utf8, buffer, bytes);
			offset = utf8 + bytes;
		} else if (utf8Length >= utf8TotalBytes) {
			char* newUTF8 = realloc(utf8, utf8TotalBytes * 2);
			if (newUTF8 != utf8) {
				free(utf8);
			}
			if (!newUTF8) {
				return 0;
			}
			offset = offset - utf8 + newUTF8;
			memcpy(offset, buffer, bytes);
			offset += bytes;
		}
	}

	char* newUTF8 = realloc(utf8, utf8Length + 1);
	if (newUTF8 != utf8) {
		free(utf8);
	}
	newUTF8[utf8Length] = '\0';
	return newUTF8;
}
