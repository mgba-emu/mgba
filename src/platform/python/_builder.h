#define COMMON_H
#define extern
#define _TIME_H_
#define _SYS_TIME_H_
#define ATTRIBUTE_FORMAT(X, Y, Z)
#define DECL_BITFIELD(newtype, oldtype) typedef oldtype newtype
#define DECL_BIT(type, name, bit)
#define DECL_BITS(type, name, bit, nbits)
typedef long time_t;
typedef ... va_list;
#include <limits.h>
#include "core/core.h"
#ifdef M_CORE_GBA
#include "arm/arm.h"
#include "gba/gba.h"
#endif
#ifdef M_CORE_GB
#include "lr35902/lr35902.h"
#include "gb/gb.h"
#endif
