#ifndef COMMON_H
#define COMMON_H

#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define UNUSED(V) (void)(V)

#ifdef __BIG_ENDIAN__

#if defined(__PPC__) || defined(__POWERPC__)
#define SWAP_32(DEST, VAR) asm(\
	"rlwinm %0, %1, 8, 24, 31\n" \
	"rlwimi %0, %1, 24, 16, 23\n" \
	"rlwimi %0, %1, 8, 8, 15\n" \
	"rlwimi %0, %1, 24, 0, 7\n" \
	: "+r"(DEST) : "r"(VAR) : )

#define SWAP_16(DEST, VAR) asm(\
	"rlwinm %0, %1, 24, 24, 31\n" \
	"rlwimi %0, %1, 8, 16, 23\n" \
	: "+r"(DEST) : "r"(VAR) : )
#endif

#define LOAD_32(DEST, ADDR, ARR) { \
	uint32_t _tmp = ((uint32_t*) ARR)[(ADDR) >> 2]; \
	SWAP_32(DEST, _tmp); \
}

#define LOAD_16(DEST, ADDR, ARR) { \
	uint16_t _tmp = ((uint16_t*) ARR)[(ADDR) >> 1]; \
	SWAP_16(DEST, _tmp); \
}

#define STORE_32(SRC, ADDR, ARR) { \
	uint32_t _tmp; \
	SWAP_32(_tmp, SRC); \
	((uint32_t*) ARR)[(ADDR) >> 2] = _tmp; \
}

#define STORE_16(SRC, ADDR, ARR) { \
	uint16_t _tmp; \
	SWAP_16(_tmp, SRC); \
	((uint16_t*) ARR)[(ADDR) >> 2] = _tmp; \
}

#else
#define LOAD_32(DEST, ADDR, ARR) DEST = ((uint32_t*) ARR)[(ADDR) >> 2]
#define LOAD_16(DEST, ADDR, ARR) DEST = ((uint16_t*) ARR)[(ADDR) >> 1]
#define STORE_32(SRC, ADDR, ARR) ((uint32_t*) ARR)[(ADDR) >> 2] = SRC
#define STORE_16(SRC, ADDR, ARR) ((uint16_t*) ARR)[(ADDR) >> 1] = SRC
#endif

#endif
