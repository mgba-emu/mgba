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

#if defined(__PPC__) || defined(__POWERPC__)
#define LOAD_32(DEST, ADDR, ARR) { \
	uint32_t _tmp = (ADDR); \
	asm("lwbrx %0, %1, %2" : "=r"(DEST) : "r"(_tmp), "p"(ARR)); \
}

#define LOAD_16(DEST, ADDR, ARR) { \
	uint32_t _tmp = (ADDR); \
	asm("lhbrx %0, %1, %2" : "=r"(DEST) : "r"(_tmp), "p"(ARR)); \
}

#define STORE_32(SRC, ADDR, ARR) { \
	uint32_t _tmp = (ADDR); \
	asm("stwbrx %0, %1, %2" : : "r"(SRC), "r"(_tmp), "p"(ARR)); \
}

#define STORE_16(SRC, ADDR, ARR) { \
	uint32_t _tmp = (ADDR); \
	asm("sthbrx %0, %1, %2" : : "r"(SRC), "r"(_tmp), "p"(ARR)); \
}
#else
#define LOAD_32(DEST, ADDR, ARR) DEST = ((uint32_t*) ARR)[(ADDR) >> 2]
#define LOAD_16(DEST, ADDR, ARR) DEST = ((uint16_t*) ARR)[(ADDR) >> 1]
#define STORE_32(SRC, ADDR, ARR) ((uint32_t*) ARR)[(ADDR) >> 2] = SRC
#define STORE_16(SRC, ADDR, ARR) ((uint16_t*) ARR)[(ADDR) >> 1] = SRC
#endif

#define MAKE_MASK(START, END) (((1 << ((END) - (START))) - 1) << (START))
#define CHECK_BITS(SRC, START, END) ((SRC) & MAKE_MASK(START, END))
#define EXT_BITS(SRC, START, END) (((SRC) >> (START)) & ((1 << ((END) - (START))) - 1))
#define INS_BITS(SRC, START, END, BITS) (CLEAR_BITS(SRC, START, END) | (((BITS) << (START)) & MAKE_MASK(START, END)))
#define CLEAR_BITS(SRC, START, END) ((SRC) & ~MAKE_MASK(START, END))
#define FILL_BITS(SRC, START, END) ((SRC) | MAKE_MASK(START, END))

#define DECL_BITFIELD(NAME, TYPE) typedef TYPE NAME

#define DECL_BITS(TYPE, FIELD, START, SIZE) \
	static inline TYPE TYPE ## Is ## FIELD (TYPE src) { \
		return CHECK_BITS(src, (START), (START) + (SIZE)); \
	} \
	static inline TYPE TYPE ## Get ## FIELD (TYPE src) { \
		return EXT_BITS(src, (START), (START) + (SIZE)); \
	} \
	static inline TYPE TYPE ## Clear ## FIELD (TYPE src) { \
		return CLEAR_BITS(src, (START), (START) + (SIZE)); \
	} \
	static inline TYPE TYPE ## Fill ## FIELD (TYPE src) { \
		return FILL_BITS(src, (START), (START) + (SIZE)); \
	} \
	static inline TYPE TYPE ## Set ## FIELD (TYPE src, TYPE bits) { \
		return INS_BITS(src, (START), (START) + (SIZE), bits); \
	}

#define DECL_BIT(TYPE, FIELD, BIT) DECL_BITS(TYPE, FIELD, BIT, 1)
#endif
