/* Copyright (c) 2013-2024 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_SIO_TEST32_H
#define GBA_SIO_TEST32_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/gba/interface.h>
#include <mgba-util/socket.h>

// NORMAL_32 TCP bridge driver. On each transfer it forwards the GBA's outbound
// word over a TCP socket and hands back whatever the server replies. This is
// the emulated stand-in for the eventual hardware device (RP2040W).
struct GBASIOTest32 {
	struct GBASIODriver d;
	Socket socket;          // TCP connection to the bridge/test server (INVALID until connected)
	bool active;            // true between SESSION_OPEN and SESSION_CLOSE; dormant otherwise
	bool haveReply;         // whether sendValue holds a fresh reply (else GBA gets open bus)
	uint8_t frameRemaining; // payload words still expected (0 = awaiting a frame header)
	uint8_t frameLen;       // payload word count of the current frame
	uint8_t frameFill;      // bytes accumulated into frameData so far
	uint8_t frameData[128]; // accumulated payload bytes (up to 32 words / 128 bytes)
	uint8_t respData[128];  // buffered server response (up to 32 words / 128 bytes)
	uint8_t respWords;      // total response words for this reply (for the feeder index)
	uint8_t respRemaining;  // response words still to clock back to the GBA (0 = none pending)
	uint32_t lastSent;      // last 32-bit word the GBA transmitted
	uint32_t sendValue;     // reply computed for THIS word (handed back one transfer later)
	uint32_t returnValue;   // ONE-TRANSFER LAG: the reply actually returned this transfer (prev word's)
	bool returnHave;        // whether returnValue is a fresh reply (else open bus)
};

void GBASIOTest32Create(struct GBASIOTest32* test);

CXX_GUARD_END

#endif
