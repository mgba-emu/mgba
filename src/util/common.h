/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef COMMON_H
#define COMMON_H

#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#ifdef _WIN64
typedef int64_t off_t;
typedef int64_t ssize_t;
#else
typedef int32_t off_t;
typedef int32_t ssize_t;
#endif
#define restrict __restrict
#define SSIZE_MAX ((ssize_t) SIZE_MAX)
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define ftruncate _chsize
#elif defined(__wii__)
typedef int32_t ssize_t;
#define SSIZE_MAX ((ssize_t) SIZE_MAX)
#else
#include <strings.h>
#include <unistd.h>
#endif

#include "version.h"

#define UNUSED(V) (void)(V)

#ifndef M_PI
#define M_PI 3.141592654f
#endif

#if defined(__PPC__) || defined(__POWERPC__)
#define LOAD_32LE(DEST, ADDR, ARR) { \
	uint32_t _addr = (ADDR); \
	void* _ptr = (ARR); \
	__asm__("lwbrx %0, %1, %2" : "=r"(DEST) : "b"(_ptr), "r"(_addr)); \
}

#define LOAD_16LE(DEST, ADDR, ARR) { \
	uint32_t _addr = (ADDR); \
	void* _ptr = (ARR); \
	__asm__("lhbrx %0, %1, %2" : "=r"(DEST) : "b"(_ptr), "r"(_addr)); \
}

#define STORE_32LE(SRC, ADDR, ARR) { \
	uint32_t _addr = (ADDR); \
	void* _ptr = (ARR); \
	__asm__("stwbrx %0, %1, %2" : : "r"(SRC), "b"(_ptr), "r"(_addr)); \
}

#define STORE_16LE(SRC, ADDR, ARR) { \
	uint32_t _addr = (ADDR); \
	void* _ptr = (ARR); \
	__asm__("sthbrx %0, %1, %2" : : "r"(SRC), "b"(_ptr), "r"(_addr)); \
}
#else
#define LOAD_32LE(DEST, ADDR, ARR) DEST = ((uint32_t*) ARR)[(ADDR) >> 2]
#define LOAD_16LE(DEST, ADDR, ARR) DEST = ((uint16_t*) ARR)[(ADDR) >> 1]
#define STORE_32LE(SRC, ADDR, ARR) ((uint32_t*) ARR)[(ADDR) >> 2] = SRC
#define STORE_16LE(SRC, ADDR, ARR) ((uint16_t*) ARR)[(ADDR) >> 1] = SRC
#endif

#define MAKE_MASK(START, END) (((1 << ((END) - (START))) - 1) << (START))
#define CHECK_BITS(SRC, START, END) ((SRC) & MAKE_MASK(START, END))
#define EXT_BITS(SRC, START, END) (((SRC) >> (START)) & ((1 << ((END) - (START))) - 1))
#define INS_BITS(SRC, START, END, BITS) (CLEAR_BITS(SRC, START, END) | (((BITS) << (START)) & MAKE_MASK(START, END)))
#define CLEAR_BITS(SRC, START, END) ((SRC) & ~MAKE_MASK(START, END))
#define FILL_BITS(SRC, START, END) ((SRC) | MAKE_MASK(START, END))

#ifdef _MSC_VER
#define ATTRIBUTE_UNUSED
#define ATTRIBUTE_FORMAT(X, Y, Z)
#else
#define ATTRIBUTE_UNUSED __attribute__((unused))
#define ATTRIBUTE_FORMAT(X, Y, Z) __attribute__((format(X, Y, Z)))
#endif

#define DECL_BITFIELD(NAME, TYPE) typedef TYPE NAME

#define DECL_BITS(TYPE, FIELD, START, SIZE) \
	ATTRIBUTE_UNUSED static inline TYPE TYPE ## Is ## FIELD (TYPE src) { \
		return CHECK_BITS(src, (START), (START) + (SIZE)); \
	} \
	ATTRIBUTE_UNUSED static inline TYPE TYPE ## Get ## FIELD (TYPE src) { \
		return EXT_BITS(src, (START), (START) + (SIZE)); \
	} \
	ATTRIBUTE_UNUSED static inline TYPE TYPE ## Clear ## FIELD (TYPE src) { \
		return CLEAR_BITS(src, (START), (START) + (SIZE)); \
	} \
	ATTRIBUTE_UNUSED static inline TYPE TYPE ## Fill ## FIELD (TYPE src) { \
		return FILL_BITS(src, (START), (START) + (SIZE)); \
	} \
	ATTRIBUTE_UNUSED static inline TYPE TYPE ## Set ## FIELD (TYPE src, TYPE bits) { \
		return INS_BITS(src, (START), (START) + (SIZE), bits); \
	}

#define DECL_BIT(TYPE, FIELD, BIT) DECL_BITS(TYPE, FIELD, BIT, 1)

#ifndef _MSC_VER
#define LIKELY(X) __builtin_expect(!!(X), 1)
#define UNLIKELY(X) __builtin_expect(!!(X), 0)
#else
#define LIKELY(X) (!!(X))
#define UNLIKELY(X) (!!(X))
#endif

#define ROR(I, ROTATE) ((((uint32_t) (I)) >> ROTATE) | ((uint32_t) (I) << ((-ROTATE) & 31)))

#endif
