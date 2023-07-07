#define COMMON_H
#define PNG_H
#define OPAQUE_THREADING
#define _SYS_TIME_H
#define _SYS_TIME_H_
#define _TIME_H
#define _TIME_H_
#define MGBA_EXPORT

#define ATTRIBUTE_FORMAT(X, Y, Z)
#define DECL_BITFIELD(newtype, oldtype) typedef oldtype newtype
#define DECL_BIT(type, field, bit) DECL_BITS(type, field, bit, 1)
#define DECL_BITS(TYPE, FIELD, START, SIZE) \
	TYPE TYPE ## Is ## FIELD (TYPE); \
	TYPE TYPE ## Get ## FIELD (TYPE); \
	TYPE TYPE ## Clear ## FIELD (TYPE); \
	TYPE TYPE ## Fill ## FIELD (TYPE); \
	TYPE TYPE ## Set ## FIELD (TYPE, TYPE); \
	TYPE TYPE ## TestFill ## FIELD (TYPE, bool);

#define CXX_GUARD_START
#define CXX_GUARD_END

#define PYCPARSE
#define va_list void*

typedef int... time_t;
typedef int... off_t;
typedef ...* png_structp;
typedef ...* png_infop;
typedef ...* png_unknown_chunkp;

void free(void*);

#include <limits.h>

#include <mgba/flags.h>

#include <mgba/core/blip_buf.h>
#include <mgba/core/cache-set.h>
#include <mgba/core/core.h>
#include <mgba/core/map-cache.h>
#include <mgba/core/mem-search.h>
#include <mgba/core/thread.h>
#include <mgba/core/version.h>

#define PYEXPORT extern "Python+C"
#include "platform/python/core.h"
#include "platform/python/log.h"
#include "platform/python/sio.h"
#include "platform/python/vfs-py.h"
#undef PYEXPORT

#ifdef USE_PNG
#include <mgba-util/image/png-io.h>
#endif
#ifdef M_CORE_GBA
#include <mgba/gba/interface.h>
#include <mgba/internal/arm/arm.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/input.h>
#include <mgba/internal/gba/renderers/cache-set.h>
#endif
#ifdef M_CORE_GB
#include <mgba/internal/sm83/sm83.h>
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gba/input.h>
#include <mgba/internal/gb/renderers/cache-set.h>
#endif
#ifdef USE_DEBUGGERS
#include <mgba/debugger/debugger.h>
#include <mgba/internal/debugger/cli-debugger.h>
#endif
