/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef UTIL_MATH_H
#define UTIL_MATH_H

#include <mgba-util/common.h>

CXX_GUARD_START

static inline uint32_t popcount32(unsigned bits) {
	bits = bits - ((bits >> 1) & 0x55555555);
	bits = (bits & 0x33333333) + ((bits >> 2) & 0x33333333);
	return (((bits + (bits >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;
}

static inline unsigned clz32(uint32_t bits) {
#if defined(__GNUC__) || __clang__
	if (!bits) {
		return 32;
	}
	return __builtin_clz(bits);
#else
	static const int table[256] = {
		8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
		3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
		2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
		2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};

	if (bits & 0xFF000000) {
		return table[bits >> 24];
	} else if (bits & 0x00FF0000) {
		return table[bits >> 16] + 8;
	} else if (bits & 0x0000FF00) {
		return table[bits >> 8] + 16;
	}
	return table[bits] + 24;
#endif
}

static inline unsigned clz64(uint64_t bits) {
#if defined(__GNUC__) || __clang__
	if (!bits) {
		return 64;
	}
	return __builtin_clzll(bits);
#else
	static const int table[256] = {
		8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
		3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
		2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
		2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};

	if (bits & 0xFF00000000000000) {
		return table[bits >> 56];
	} else if (bits & 0x00FF000000000000) {
		return table[bits >> 48] + 8;
	} else if (bits & 0x0000FF0000000000) {
		return table[bits >> 40] + 16;
	} else if (bits & 0x000000FF00000000) {
		return table[bits >> 32] + 24;
	} else if (bits & 0x00000000FF000000) {
		return table[bits >> 24] + 32;
	} else if (bits & 0x0000000000FF0000) {
		return table[bits >> 16] + 40;
	} else if (bits & 0x000000000000FF00) {
		return table[bits >> 8] + 48;
	}
	return table[bits] + 56;
#endif
}

static inline uint32_t toPow2(uint32_t bits) {
	if (!bits) {
		return 0;
	}
	unsigned lz = clz32(bits - 1);
	return 1 << (32 - lz);
}

static inline int reduceFraction(int* num, int* den) {
	int n = *num;
	int d = *den;
	while (d != 0) {
		int temp = n % d;
		n = d;
		d = temp;
	}
	*num /= n;
	*den /= n;
	return n;
}

CXX_GUARD_END

#endif
