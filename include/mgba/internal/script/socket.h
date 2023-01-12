/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_SCRIPT_SOCKET_H
#define M_SCRIPT_SOCKET_H

#include <mgba-util/common.h>

CXX_GUARD_START

enum mSocketErrorCode {
	mSCRIPT_SOCKERR_UNKNOWN_ERROR = -1,
	mSCRIPT_SOCKERR_OK = 0,
	mSCRIPT_SOCKERR_AGAIN,
	mSCRIPT_SOCKERR_ADDRESS_IN_USE,
	mSCRIPT_SOCKERR_CONNECTION_REFUSED,
	mSCRIPT_SOCKERR_DENIED,
	mSCRIPT_SOCKERR_FAILED,
	mSCRIPT_SOCKERR_NETWORK_UNREACHABLE,
	mSCRIPT_SOCKERR_NOT_FOUND,
	mSCRIPT_SOCKERR_NO_DATA,
	mSCRIPT_SOCKERR_OUT_OF_MEMORY,
	mSCRIPT_SOCKERR_TIMEOUT,
	mSCRIPT_SOCKERR_UNSUPPORTED,
};

CXX_GUARD_END

#endif
