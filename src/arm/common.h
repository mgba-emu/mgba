#ifndef COMMON_H
#define COMMON_H

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define UNUSED(V) (void)(V)

#ifdef __POWERPC__
#define LOAD_32(DEST, ADDR, ARR) asm("lwbrx %0, %1, %2" : "=r"(DEST) : "r"(ADDR), "r"(ARR))
#define LOAD_16(DEST, ADDR, ARR) asm("lhbrx %0, %1, %2" : "=r"(DEST) : "r"(ADDR), "r"(ARR))
#define STORE_32(SRC, ADDR, ARR) asm("stwbrx %0, %1, %2" : : "r"(SRC), "r"(ADDR), "r"(ARR))
#define STORE_16(SRC, ADDR, ARR) asm("sthbrx %0, %1, %2" : : "r"(SRC), "r"(ADDR), "r"(ARR))
#else
#define LOAD_32(DEST, ADDR, ARR) DEST = ((uint32_t*) ARR)[(ADDR) >> 2]
#define LOAD_16(DEST, ADDR, ARR) DEST = ((uint16_t*) ARR)[(ADDR) >> 1]
#define STORE_32(SRC, ADDR, ARR) ((uint32_t*) ARR)[(ADDR) >> 2] = SRC
#define STORE_16(SRC, ADDR, ARR) ((uint16_t*) ARR)[(ADDR) >> 1] = SRC
#endif

#endif
