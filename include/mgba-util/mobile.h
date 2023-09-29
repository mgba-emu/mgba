#ifndef MOBILE_H
#define MOBILE_H

#ifdef USE_LIBMOBILE

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba-util/socket.h>
#include <mobile.h>

struct MobileAdapterGB {
	void *p;

	struct mobile_adapter *adapter;
	uint8_t config[MOBILE_CONFIG_SIZE];
	struct {
		Socket fd;
		enum mobile_socktype socktype;
	} socket[MOBILE_MAX_CONNECTIONS];
	int serial;
	char number[2][MOBILE_MAX_NUMBER_SIZE + 1];
};

struct mobile_adapter* MobileAdapterGBNew(struct MobileAdapterGB *mobile);

CXX_GUARD_END

#endif

#endif
