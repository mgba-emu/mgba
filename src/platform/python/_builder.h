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

#define CXX_GUARD_START
#define CXX_GUARD_END

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

#include <mgba/core/core.h>
#include <mgba/core/mem-search.h>
#include <mgba/core/tile-cache.h>
#include <mgba/core/version.h>

#define PYEXPORT extern "Python+C"
#include "platform/python/log.h"
#include "platform/python/sio.h"
#include "platform/python/vfs-py.h"
#undef PYEXPORT

#ifdef USE_PNG
#include <mgba-util/png-io.h>
#endif
#ifdef M_CORE_GBA
#include <mgba/internal/arm/arm.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/input.h>
#include <mgba/internal/gba/renderers/tile-cache.h>
#endif
#ifdef M_CORE_GB
#include <mgba/internal/lr35902/lr35902.h>
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gba/input.h>
#include <mgba/internal/gb/renderers/tile-cache.h>
#endif
