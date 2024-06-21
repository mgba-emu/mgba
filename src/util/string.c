/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/string.h>

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

#ifndef HAVE_STRDUP
char* strdup(const char* str) {
	size_t len = strlen(str);
	char* out = malloc(len + 1);
	strncpy(out, str, len);
	out[len] = '\0';
	return out;
}
#endif

#ifndef HAVE_STRLCPY
size_t strlcpy(char* restrict dst, const char* restrict src, size_t dstsize) {
	size_t i = 0;
	for (; src[i] && dstsize > 1; ++i) {
		dst[i] = src[i];
		--dstsize;
	}
	if (dstsize) {
		dst[i] = '\0';
	}
	while (src[i]) {
		++i;
	}
	return i;
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

bool endswith(const char* restrict s1, const char* restrict end) {
	size_t len = strlen(s1);
	size_t endLen = strlen(end);
	if (len < endLen) {
		return false;
	}
	return strcmp(&s1[len - endLen], end) == 0;
}

bool startswith(const char* restrict s1, const char* restrict start) {
	size_t len = strlen(s1);
	size_t startLen = strlen(start);
	if (len < startLen) {
		return false;
	}
	return strncmp(s1, start, startLen) == 0;
}

uint32_t utf16Char(const uint16_t** unicode, size_t* length) {
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

static const uint8_t _utf8len[0x40] = {
	/* 0000 xxxx */ 1, 1, 1, 1,
	/* 0001 xxxx */ 1, 1, 1, 1,
	/* 0010 xxxx */ 1, 1, 1, 1,
	/* 0011 xxxx */ 1, 1, 1, 1,
	/* 0100 xxxx */ 1, 1, 1, 1,
	/* 0101 xxxx */ 1, 1, 1, 1,
	/* 0110 xxxx */ 1, 1, 1, 1,
	/* 0111 xxxx */ 1, 1, 1, 1,
	/* 1000 xxxx */ 0, 0, 0, 0,
	/* 1001 xxxx */ 0, 0, 0, 0,
	/* 1010 xxxx */ 0, 0, 0, 0,
	/* 1011 xxxx */ 0, 0, 0, 0,
	/* 1100 xxxx */ 2, 2, 2, 2,
	/* 1101 xxxx */ 2, 2, 2, 2,
	/* 1110 xxxx */ 3, 3, 3, 3,
	/* 1111 xxxx */ 4, 4, 0, 0
};

uint32_t utf8Char(const char** unicode, size_t* length) {
	if (*length == 0) {
		return 0;
	}
	unsigned char byte = **unicode;
	--*length;
	++*unicode;
	if (!(byte & 0x80)) {
		return byte;
	}
	uint32_t unichar;
	static const int tops[4] = { 0xC0, 0xE0, 0xF0, 0xF8 };
	size_t numBytes = _utf8len[byte >> 2];
	unichar = byte & ~tops[numBytes - 1];
	if (numBytes == 0) {
		return 0xFFFD;
	}
	if (*length + 1 < numBytes) {
		*length = 0;
		return 0xFFFD;
	}
	size_t i;
	for (i = 1; i < numBytes; ++i) {
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

size_t toUtf8(uint32_t unichar, char* buffer) {
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

size_t toUtf16(uint32_t unichar, uint16_t* buffer) {
	if (unichar < 0xD800) {
		buffer[0] = unichar;
		return 1;
	}
	if (unichar < 0xE000) {
		// Orphan surrogate, invalid
		return 0;
	}
	if (unichar < 0x10000) {
		buffer[0] = unichar;
		return 1;
	}
	if (unichar < 0x110000) {
		unichar -= 0x10000;
		buffer[0] = 0xD800 | (unichar >> 10);
		buffer[1] = 0xDC00 | (unichar & 0x3FF);
		return 2;
	}

	// Invalid code point
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
		char1 = utf16Char(&utf16, &utf16Length);
		char2 = utf8Char(&utf8, &utf8Length);
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
	char* offset = 0;
	char buffer[4];
	size_t utf8TotalBytes = 0;
	size_t utf8Length = 0;
	while (true) {
		if (length == 0) {
			break;
		}
		uint32_t unichar = utf16Char(&utf16, &length);
		size_t bytes = toUtf8(unichar, buffer);
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
			ptrdiff_t o = offset - utf8;
			char* newUTF8 = realloc(utf8, utf8TotalBytes * 2);
			offset = o + newUTF8;
			if (!newUTF8) {
				free(utf8);
				return 0;
			}
			utf8 = newUTF8;
			memcpy(offset, buffer, bytes);
			offset += bytes;
		}
	}

	char* newUTF8 = realloc(utf8, utf8Length + 1);
	if (!newUTF8) {
		free(utf8);
		return 0;
	}
	newUTF8[utf8Length] = '\0';
	return newUTF8;
}

char* latin1ToUtf8(const char* latin1, size_t length) {
	char* utf8 = NULL;
	char* utf8Offset = NULL;
	size_t offset;
	char buffer[4];
	size_t utf8TotalBytes = 0;
	size_t utf8Length = 0;
	for (offset = 0; offset < length; ++offset) {
		if (length == 0) {
			break;
		}
		uint8_t unichar = latin1[offset];
		size_t bytes = toUtf8(unichar, buffer);
		utf8Length += bytes;
		if (!utf8) {
			utf8 = malloc(length);
			if (!utf8) {
				return NULL;
			}
			utf8TotalBytes = length;
			memcpy(utf8, buffer, bytes);
			utf8Offset = utf8 + bytes;
		} else if (utf8Length < utf8TotalBytes) {
			memcpy(utf8Offset, buffer, bytes);
			utf8Offset += bytes;
		} else if (utf8Length >= utf8TotalBytes) {
			ptrdiff_t o = utf8Offset - utf8;
			char* newUTF8 = realloc(utf8, utf8TotalBytes * 2);
			utf8Offset = o + newUTF8;
			if (!newUTF8) {
				free(utf8);
				return 0;
			}
			utf8 = newUTF8;
			memcpy(utf8Offset, buffer, bytes);
			utf8Offset += bytes;
		}
	}

	char* newUTF8 = realloc(utf8, utf8Length + 1);
	if (!newUTF8) {
		free(utf8);
		return 0;
	}
	newUTF8[utf8Length] = '\0';
	return newUTF8;
}

extern const uint16_t gbkUnicodeTable[];

char* gbkToUtf8(const char* gbk, size_t length) {
	char* utf8 = NULL;
	char* utf8Offset = NULL;
	size_t offset;
	uint8_t gbk1 = 0;
	char buffer[4];
	size_t utf8TotalBytes = 0;
	size_t utf8Length = 0;
	for (offset = 0; offset < length; ++offset) {
		if (length == 0) {
			break;
		}
		unsigned unichar = 0xFFFD;
		if (!gbk1 && !(gbk[offset] & 0x80)) {
			unichar = gbk[offset];
		} else if (gbk1) {
			uint8_t gbk2 = gbk[offset];
			if (gbk2 >= 0x40 && gbk2 != 0xFF) {
				// TODO: GB-18030 support?
				unichar = gbkUnicodeTable[gbk1 * 0xBF + gbk2 - 0x40];
			}
			gbk1 = 0;
		} else if (((uint8_t*) gbk)[offset] == 0xFF) {
			unichar = 0xFFFD;
		} else if (((uint8_t*) gbk)[offset] == 0x80) {
			unichar = 0x20AC; // Euro
		} else {
			gbk1 = ((uint8_t*) gbk)[offset] - 0x81;
			continue;
		}

		size_t bytes = toUtf8(unichar, buffer);
		utf8Length += bytes;
		if (!utf8) {
			utf8 = malloc(length);
			if (!utf8) {
				return NULL;
			}
			utf8TotalBytes = length;
			memcpy(utf8, buffer, bytes);
			utf8Offset = utf8 + bytes;
		} else if (utf8Length < utf8TotalBytes) {
			memcpy(utf8Offset, buffer, bytes);
			utf8Offset += bytes;
		} else if (utf8Length >= utf8TotalBytes) {
			ptrdiff_t o = utf8Offset - utf8;
			char* newUTF8 = realloc(utf8, utf8TotalBytes * 2);
			utf8Offset = o + newUTF8;
			if (!newUTF8) {
				free(utf8);
				return 0;
			}
			utf8 = newUTF8;
			memcpy(utf8Offset, buffer, bytes);
			utf8Offset += bytes;
		}
	}

	char* newUTF8 = realloc(utf8, utf8Length + 1);
	if (!newUTF8) {
		free(utf8);
		return 0;
	}
	newUTF8[utf8Length] = '\0';
	return newUTF8;
}

size_t utf8strlen(const char* string) {
	size_t size = 0;
	for (size = 0; *string; ++size) {
		size_t numBytes = 1;
		if (*string & 0x80) {
			numBytes = _utf8len[((uint8_t) *string) >> 2];
			if (!numBytes) {
				numBytes = 1;
			} else {
				size_t i;
				for (i = 1; i < numBytes; ++i) {
					if ((string[i] & 0xC0) != 0x80) {
						break;
					}
				}
				numBytes = i;
			}
		}
		string += numBytes;
	}
	return size;
}

int hexDigit(char digit) {
	switch (digit) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		return digit - '0';

	case 'a':
	case 'b':
	case 'c':
	case 'd':
	case 'e':
	case 'f':
		return digit - 'a' + 10;

	case 'A':
	case 'B':
	case 'C':
	case 'D':
	case 'E':
	case 'F':
		return digit - 'A' + 10;

	default:
		return -1;
	}
}

const char* hex32(const char* line, uint32_t* out) {
	uint32_t value = 0;
	int i;
	for (i = 0; i < 8; ++i, ++line) {
		char digit = *line;
		value <<= 4;
		int nybble = hexDigit(digit);
		if (nybble < 0) {
			return 0;
		}
		value |= nybble;
	}
	*out = value;
	return line;
}

const char* hex24(const char* line, uint32_t* out) {
	uint32_t value = 0;
	int i;
	for (i = 0; i < 6; ++i, ++line) {
		char digit = *line;
		value <<= 4;
		int nybble = hexDigit(digit);
		if (nybble < 0) {
			return 0;
		}
		value |= nybble;
	}
	*out = value;
	return line;
}

const char* hex16(const char* line, uint16_t* out) {
	uint16_t value = 0;
	*out = 0;
	int i;
	for (i = 0; i < 4; ++i, ++line) {
		char digit = *line;
		value <<= 4;
		int nybble = hexDigit(digit);
		if (nybble < 0) {
			return 0;
		}
		value |= nybble;
	}
	*out = value;
	return line;
}

const char* hex12(const char* line, uint16_t* out) {
	uint16_t value = 0;
	*out = 0;
	int i;
	for (i = 0; i < 3; ++i, ++line) {
		char digit = *line;
		value <<= 4;
		int nybble = hexDigit(digit);
		if (nybble < 0) {
			return 0;
		}
		value |= nybble;
	}
	*out = value;
	return line;
}

const char* hex8(const char* line, uint8_t* out) {
	uint8_t value = 0;
	*out = 0;
	int i;
	for (i = 0; i < 2; ++i, ++line) {
		char digit = *line;
		value <<= 4;
		int nybble = hexDigit(digit);
		if (nybble < 0) {
			return 0;
		}
		value |= nybble;
	}
	*out = value;
	return line;
}

const char* hex4(const char* line, uint8_t* out) {
	uint8_t value = 0;
	*out = 0;
	char digit = *line;
	int nybble = hexDigit(digit);
	if (nybble < 0) {
		return 0;
	}
	value |= nybble;
	*out = value;
	return line;
}

void rtrim(char* string) {
	if (!*string) {
		return;
	}
	char* end = string + strlen(string) - 1;
	while (isspace((int) *end) && end >= string) {
		*end = '\0';
		--end;
	}
}

ssize_t parseQuotedString(const char* unparsed, ssize_t unparsedLen, char* parsed, ssize_t parsedLen) {
	memset(parsed, 0, parsedLen);
	bool escaped = false;
	char start = '\0';
	ssize_t len = 0;
	ssize_t i;
	for (i = 0; i < unparsedLen && len < parsedLen; ++i) {
		if (i == 0) {
			switch (unparsed[0]) {
			case '"':
			case '\'':
				start = unparsed[0];
				break;
			default:
				return -1;
			}
			continue;
		}
		if (escaped) {
			switch (unparsed[i]) {
			case 'n':
				parsed[len] = '\n';
				break;
			case 'r':
				parsed[len] = '\r';
				break;
			case '\\':
				parsed[len] = '\\';
				break;
			case '\'':
				parsed[len] = '\'';
				break;
			case '"':
				parsed[len] = '"';
				break;
			default:
				return -1;
			}
			escaped = false;
			++len;
			continue;
		}
		if (unparsed[i] == start) {
			return len;
		}
		switch (unparsed[i]) {
		case '\\':
			escaped = true;
			break;
		case '\n':
		case '\r':
			return len;
		default:
			parsed[len] = unparsed[i];
			++len;
			break;
		}
	}
	return -1;
}

bool wildcard(const char* search, const char* string) {
	while (true) {
		if (search[0] == '*') {
			while (search[0] == '*') {
				++search;
			}
			if (!search[0]) {
				return true;
			}
			while (string[0]) {
				if (string[0] == search[0] && wildcard(search, string)) {
					return true;
				}
				++string;
			}
			return false;
		} else if (!search[0]) {
			return !string[0];
		} else if (!string[0]) {
			return false;
		} else if (string[0] != search[0]) {
			return false;
		} else {
			++search;
			++string;
		}
	}
	return false;
}
