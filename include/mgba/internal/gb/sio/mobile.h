#ifndef GB_MOBILE_H
#define GB_MOBILE_H

#ifdef USE_LIBMOBILE

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/gb/interface.h>

#include <mgba-util/socket.h>
#include <mobile.h>

struct GBMobileAdapter {
	struct GBSIODriver d;
	struct mobile_adapter *adapter;
	uint8_t config[MOBILE_CONFIG_SIZE];
	struct {
		Socket fd;
		enum mobile_socktype socktype;
		enum mobile_addrtype addrtype;
		unsigned bindport;
	} socket[MOBILE_MAX_CONNECTIONS];
	unsigned timeLatch[MOBILE_MAX_TIMERS];
	int serial;
	uint8_t nextData[2];
	char number[2][MOBILE_MAX_NUMBER_SIZE + 1];
};

void GBMobileAdapterCreate(struct GBMobileAdapter*);

CXX_GUARD_END

#endif

#endif
