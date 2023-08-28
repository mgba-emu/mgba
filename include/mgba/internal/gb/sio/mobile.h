#ifndef GB_MOBILE_H
#define GB_MOBILE_H

#ifdef USE_LIBMOBILE

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/gb/interface.h>
#include <mgba-util/mobile.h>

struct GBMobileAdapter {
	struct GBSIODriver d;
	struct MobileAdapterGB m;
	uint8_t nextData[2];
};

void GBMobileAdapterCreate(struct GBMobileAdapter*);

CXX_GUARD_END

#endif

#endif
