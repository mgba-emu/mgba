#ifndef GBA_MOBILE_H
#define GBA_MOBILE_H

#ifdef USE_LIBMOBILE

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/gba/interface.h>
#include <mgba-util/mobile.h>

struct GBASIOMobileAdapter {
	struct GBASIODriver d;
	struct mTimingEvent event;
	struct MobileAdapterGB m;
	uint32_t nextData;
};

void GBASIOMobileAdapterCreate(struct GBASIOMobileAdapter*);

CXX_GUARD_END

#endif

#endif
