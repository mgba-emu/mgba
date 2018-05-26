/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef COMMON_H
#define COMMON_H

#ifdef __cplusplus
#define CXX_GUARD_START extern "C" {
#define CXX_GUARD_END }
#else
#define CXX_GUARD_START
#define CXX_GUARD_END
#endif

CXX_GUARD_START

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
#include <time.h>

#ifdef _WIN32
// WinSock2 gets very angry if it's included too late
#include <winsock2.h>
#endif
#ifdef _MSC_VER
#include <Windows.h>
#include <sys/types.h>
typedef intptr_t ssize_t;
#define PATH_MAX MAX_PATH
#define restrict __restrict
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define ftruncate _chsize
#define snprintf _snprintf
#define strdup _strdup
#define lseek _lseek
#define O_ACCMODE (O_RDONLY|O_WRONLY|O_RDWR)
#elif defined(__wii__)
#include <sys/time.h>
typedef intptr_t ssize_t;
#else
#include <strings.h>
#include <unistd.h>
#include <sys/time.h>
#endif

#ifdef PSP2
// For PATH_MAX on modern toolchains
#include <sys/syslimits.h>
#endif

#ifndef SSIZE_MAX
#define SSIZE_MAX ((ssize_t) (SIZE_MAX >> 1))
#endif

#ifndef UNUSED
#define UNUSED(V) (void)(V)
#endif

#ifndef M_PI
#define M_PI 3.141592654f
#endif

#if !defined(_MSC_VER) && (defined(__llvm__) || (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7))
#define ATOMIC_STORE(DST, SRC) __atomic_store_n(&DST, SRC, __ATOMIC_RELEASE)
#define ATOMIC_LOAD(DST, SRC) DST = __atomic_load_n(&SRC, __ATOMIC_ACQUIRE)
#define ATOMIC_ADD(DST, OP) __atomic_add_fetch(&DST, OP, __ATOMIC_RELEASE)
#define ATOMIC_OR(DST, OP) __atomic_or_fetch(&DST, OP, __ATOMIC_RELEASE)
#define ATOMIC_AND(DST, OP) __atomic_and_fetch(&DST, OP, __ATOMIC_RELEASE)
#define ATOMIC_CMPXCHG(DST, EXPECTED, SRC) __atomic_compare_exchange_n(&DST, &EXPECTED, SRC, true,__ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)
#else
// TODO
#define ATOMIC_STORE(DST, SRC) DST = SRC
#define ATOMIC_LOAD(DST, SRC) DST = SRC
#define ATOMIC_ADD(DST, OP) DST += OP
#define ATOMIC_OR(DST, OP) DST |= OP
#define ATOMIC_AND(DST, OP) DST &= OP
#define ATOMIC_CMPXCHG(DST, EXPECTED, OP) ((DST == EXPECTED) ? ((DST = OP), true) : false)
#endif

#if defined(_3DS) || defined(GEKKO) || defined(PSP2)
// newlib doesn't support %z properly by default
#define PRIz ""
#elif defined(_WIN64)
#define PRIz "ll"
#elif defined(_WIN32)
#define PRIz ""
#else
#define PRIz "z"
#endif

#if defined __BIG_ENDIAN__
#define LOAD_32BE(DEST, ADDR, ARR) DEST = *(uint32_t*) ((uintptr_t) (ARR) + (size_t) (ADDR))
#if defined(__PPC__) || defined(__POWERPC__)
#define LOAD_32LE(DEST, ADDR, ARR) { \
	uint32_t _addr = (ADDR); \
	const void* _ptr = (ARR); \
	__asm__("lwbrx %0, %1, %2" : "=r"(DEST) : "b"(_ptr), "r"(_addr)); \
}

#define LOAD_16LE(DEST, ADDR, ARR) { \
	uint32_t _addr = (ADDR); \
	const void* _ptr = (ARR); \
	__asm__("lhbrx %0, %1, %2" : "=r"(DEST) : "b"(_ptr), "r"(_addr)); \
}

#define STORE_32LE(SRC, ADDR, ARR) { \
	uint32_t _addr = (ADDR); \
	void* _ptr = (ARR); \
	__asm__("stwbrx %0, %1, %2" : : "r"(SRC), "b"(_ptr), "r"(_addr) : "memory"); \
}

#define STORE_16LE(SRC, ADDR, ARR) { \
	uint32_t _addr = (ADDR); \
	void* _ptr = (ARR); \
	__asm__("sthbrx %0, %1, %2" : : "r"(SRC), "b"(_ptr), "r"(_addr) : "memory"); \
}

#define LOAD_64LE(DEST, ADDR, ARR) { \
	uint32_t _addr = (ADDR); \
	union { \
		struct { \
			uint32_t hi; \
			uint32_t lo; \
		}; \
		uint64_t b64; \
	} bswap; \
	const void* _ptr = (ARR); \
	__asm__( \
		"lwbrx %0, %2, %3 \n" \
		"lwbrx %1, %2, %4 \n" \
		: "=&r"(bswap.lo), "=&r"(bswap.hi) : "b"(_ptr), "r"(_addr), "r"(_addr + 4)) ; \
	DEST = bswap.b64; \
}

#define STORE_64LE(SRC, ADDR, ARR) { \
	uint32_t _addr = (ADDR); \
	union { \
		struct { \
			uint32_t hi; \
			uint32_t lo; \
		}; \
		uint64_t b64; \
	} bswap = { .b64 = SRC }; \
	const void* _ptr = (ARR); \
	__asm__( \
		"stwbrx %0, %2, %3 \n" \
		"stwbrx %1, %2, %4 \n" \
		: : "r"(bswap.hi), "r"(bswap.lo), "b"(_ptr), "r"(_addr), "r"(_addr + 4) : "memory"); \
}

#elif defined(__llvm__) || (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)
#define LOAD_64LE(DEST, ADDR, ARR) DEST = __builtin_bswap64(((uint64_t*) ARR)[(ADDR) >> 3])
#define LOAD_32LE(DEST, ADDR, ARR) DEST = __builtin_bswap32(((uint32_t*) ARR)[(ADDR) >> 2])
#define LOAD_16LE(DEST, ADDR, ARR) DEST = __builtin_bswap16(((uint16_t*) ARR)[(ADDR) >> 1])
#define STORE_64LE(SRC, ADDR, ARR) ((uint64_t*) ARR)[(ADDR) >> 3] = __builtin_bswap64(SRC)
#define STORE_32LE(SRC, ADDR, ARR) ((uint32_t*) ARR)[(ADDR) >> 2] = __builtin_bswap32(SRC)
#define STORE_16LE(SRC, ADDR, ARR) ((uint16_t*) ARR)[(ADDR) >> 1] = __builtin_bswap16(SRC)
#else
#error Big endian build not supported on this platform.
#endif
#else
#define LOAD_64LE(DEST, ADDR, ARR) DEST = *(uint64_t*) ((uintptr_t) (ARR) + (size_t) (ADDR))
#define LOAD_32LE(DEST, ADDR, ARR) DEST = *(uint32_t*) ((uintptr_t) (ARR) + (size_t) (ADDR))
#define LOAD_16LE(DEST, ADDR, ARR) DEST = *(uint16_t*) ((uintptr_t) (ARR) + (size_t) (ADDR))
#define STORE_64LE(SRC, ADDR, ARR) *(uint64_t*) ((uintptr_t) (ARR) + (size_t) (ADDR)) = SRC
#define STORE_32LE(SRC, ADDR, ARR) *(uint32_t*) ((uintptr_t) (ARR) + (size_t) (ADDR)) = SRC
#define STORE_16LE(SRC, ADDR, ARR) *(uint16_t*) ((uintptr_t) (ARR) + (size_t) (ADDR)) = SRC
#define LOAD_32BE(DEST, ADDR, ARR) DEST = __builtin_bswap32(((uint32_t*) ARR)[(ADDR) >> 2])
#endif

#define MAKE_MASK(START, END) (((1 << ((END) - (START))) - 1) << (START))
#define CHECK_BITS(SRC, START, END) ((SRC) & MAKE_MASK(START, END))
#define EXT_BITS(SRC, START, END) (((SRC) >> (START)) & ((1 << ((END) - (START))) - 1))
#define INS_BITS(SRC, START, END, BITS) (CLEAR_BITS(SRC, START, END) | (((BITS) << (START)) & MAKE_MASK(START, END)))
#define CLEAR_BITS(SRC, START, END) ((SRC) & ~MAKE_MASK(START, END))
#define FILL_BITS(SRC, START, END) ((SRC) | MAKE_MASK(START, END))
#define TEST_FILL_BITS(SRC, START, END, TEST) ((TEST) ? (FILL_BITS(SRC, START, END)) : (CLEAR_BITS(SRC, START, END)))

#ifdef _MSC_VER
#define ATTRIBUTE_UNUSED
#define ATTRIBUTE_FORMAT(X, Y, Z)
#define ATTRIBUTE_NOINLINE
#else
#define ATTRIBUTE_UNUSED __attribute__((unused))
#define ATTRIBUTE_FORMAT(X, Y, Z) __attribute__((format(X, Y, Z)))
#define ATTRIBUTE_NOINLINE __attribute__((noinline))
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
	} \
	ATTRIBUTE_UNUSED static inline TYPE TYPE ## TestFill ## FIELD (TYPE src, bool test) { \
		return TEST_FILL_BITS(src, (START), (START) + (SIZE), test); \
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

CXX_GUARD_END

#endif
