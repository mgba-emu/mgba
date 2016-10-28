#define COMMON_H
#define PNG_H
#define _SYS_TIME_H
#define _SYS_TIME_H_
#define _TIME_H
#define _TIME_H_

#define ATTRIBUTE_FORMAT(X, Y, Z)
#define DECL_BITFIELD(newtype, oldtype) typedef oldtype newtype
#define DECL_BIT(type, name, bit)
#define DECL_BITS(type, name, bit, nbits)

typedef int... time_t;
typedef int... off_t;
typedef ... va_list;
typedef ...* png_structp;
typedef ...* png_infop;
typedef ...* png_unknown_chunkp;

void free(void*);

#include <limits.h>
#undef const

#include "flags.h"

#include "core/core.h"
#include "core/tile-cache.h"
#include "platform/python/vfs-py.h"
#include "platform/python/log.h"

#ifdef USE_PNG
#include "util/png-io.h"
#endif
#ifdef M_CORE_GBA
#include "arm/arm.h"
#include "gba/gba.h"
#include "gba/input.h"
#include "gba/renderers/tile-cache.h"
#endif
#ifdef M_CORE_GB
#include "lr35902/lr35902.h"
#include "gb/gb.h"
#include "gba/input.h"
#include "gb/renderers/tile-cache.h"
#endif
