/* Copyright (c) 2013-2024 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/sio/test32.h>

#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/sio.h>

#define TEST32_HOST         0x7F000001u  // 127.0.0.1, host byte order
#define TEST32_PORT         9000         // CelioServer TCP port
#define TEST32_RECV_TIMEOUT 100          // ms to wait for the server's reply
#define TEST32_NO_DATA      0xFFFFFFFFu  // open bus returned to the GBA when we have nothing
#define TEST32_SESSION_OPEN  0x4F50454Eu // 'OPEN' — GBA opens a bridge session
#define TEST32_SESSION_CLOSE 0x434C4F53u // 'CLOS' — GBA closes the session
#define TEST32_SESSION_BYE   0x42594521u // 'BYE!' — GBA asks us to drop the TCP connection
#define TEST32_MAX_FRAME_WORDS 32        // max payload words in one frame (128 bytes)
#define TEST32_MAX_RESP_WORDS  32        // max response words clocked back (128 bytes)

static bool GBASIOTest32Init(struct GBASIODriver* driver);
static void GBASIOTest32Deinit(struct GBASIODriver* driver);
static void GBASIOTest32Reset(struct GBASIODriver* driver);
static void GBASIOTest32SetMode(struct GBASIODriver* driver, enum GBASIOMode mode);
static bool GBASIOTest32HandlesMode(struct GBASIODriver* driver, enum GBASIOMode mode);
static int GBASIOTest32ConnectedDevices(struct GBASIODriver* driver);
static int GBASIOTest32DeviceId(struct GBASIODriver* driver);
static uint16_t GBASIOTest32WriteSIOCNT(struct GBASIODriver* driver, uint16_t value);
static bool GBASIOTest32Start(struct GBASIODriver* driver);
static uint32_t GBASIOTest32FinishNormal32(struct GBASIODriver* driver);

void GBASIOTest32Create(struct GBASIOTest32* test) {
	memset(test, 0, sizeof(*test));
	test->socket = INVALID_SOCKET;
	test->d.init = GBASIOTest32Init;
	test->d.deinit = GBASIOTest32Deinit;
	test->d.reset = GBASIOTest32Reset;
	test->d.setMode = GBASIOTest32SetMode;
	test->d.handlesMode = GBASIOTest32HandlesMode;
	test->d.connectedDevices = GBASIOTest32ConnectedDevices;
	test->d.deviceId = GBASIOTest32DeviceId;
	test->d.writeSIOCNT = GBASIOTest32WriteSIOCNT;
	test->d.start = GBASIOTest32Start;
	test->d.finishNormal32 = GBASIOTest32FinishNormal32;
}

// Lazily (re)connect to the bridge server. Returns false if the server isn't up;
// connect() to localhost fails fast (ECONNREFUSED) so this never stalls.
static bool _ensureConnected(struct GBASIOTest32* test) {
	struct Address addr;
	if (!SOCKET_FAILED(test->socket)) {
		return true;
	}
	addr.version = IPV4;
	addr.ipv4 = TEST32_HOST;
	test->socket = SocketConnectTCP(TEST32_PORT, &addr);
	if (SOCKET_FAILED(test->socket)) {
		mLOG(GBA_SIO, WARN, "[test32] connect to 127.0.0.1:%i failed", TEST32_PORT);
		return false;
	}
	SocketSetBlocking(test->socket, false);
	SocketSetTCPPush(test->socket, true);
	mLOG(GBA_SIO, INFO, "[test32] connected to 127.0.0.1:%i", TEST32_PORT);
	return true;
}

static void _dropConnection(struct GBASIOTest32* test) {
	if (!SOCKET_FAILED(test->socket)) {
		SocketClose(test->socket);
		test->socket = INVALID_SOCKET;
	}
}

// Receive exactly `n` bytes with a bounded total wait.
static bool _recvBytes(struct GBASIOTest32* test, uint8_t* buf, size_t n) {
	size_t got = 0;
	while (got < n) {
		Socket r = test->socket;
		if (SocketPoll(1, &r, 0, 0, TEST32_RECV_TIMEOUT) <= 0) {
			return false; // timeout or poll error
		}
		ssize_t k = SocketRecv(test->socket, &buf[got], n - got);
		if (k > 0) {
			got += (size_t) k;
		} else if (k < 0 && SocketWouldBlock()) {
			continue;
		} else {
			return false; // connection closed or hard error
		}
	}
	return true;
}

// Read one CelioServer Message: [0x25][bufferNo][sizeHi][sizeLo][0x5F][content...].
// Scans for the 0x25 identifier first, which skips the plain-text connect greeting,
// then copies up to `outSize` content bytes into `out` (rest discarded). The full
// content length (clamped to outSize) is reported via `outLen` so the caller knows
// how many words to clock back to the GBA.
static bool _recvCelioContent(struct GBASIOTest32* test, uint8_t* out, size_t outSize, uint32_t* outLen) {
	uint8_t b;
	uint8_t hdr[4];
	uint32_t size, i;

	memset(out, 0, outSize);
	*outLen = 0;
	do {
		if (!_recvBytes(test, &b, 1)) {
			return false;
		}
	} while (b != 0x25);

	if (!_recvBytes(test, hdr, 4)) { // bufferNo, sizeHi, sizeLo, 0x5F
		return false;
	}
	size = (hdr[1] << 8) | hdr[2];
	for (i = 0; i < size; i++) {
		if (!_recvBytes(test, &b, 1)) {
			return false;
		}
		if (i < outSize) {
			out[i] = b;
		}
	}
	*outLen = (size < outSize) ? size : (uint32_t) outSize;
	return true;
}

static bool GBASIOTest32Init(struct GBASIODriver* driver) {
	struct GBASIOTest32* test = (struct GBASIOTest32*) driver;
	SocketSubsystemInit();
	test->socket = INVALID_SOCKET;
	mLOG(GBA_SIO, INFO, "[test32] driver attached");
	return true;
}

static void GBASIOTest32Deinit(struct GBASIODriver* driver) {
	struct GBASIOTest32* test = (struct GBASIOTest32*) driver;
	_dropConnection(test);
}

static void GBASIOTest32Reset(struct GBASIODriver* driver) {
	struct GBASIOTest32* test = (struct GBASIOTest32*) driver;
	test->lastSent = 0;
	test->haveReply = false;
	test->returnHave = false;
	test->active = false;
	test->frameRemaining = 0;
	test->respRemaining = 0;
	test->respWords = 0;
	// Keep any existing socket open across a game reset.
}

static void GBASIOTest32SetMode(struct GBASIODriver* driver, enum GBASIOMode mode) {
	UNUSED(driver);
	mLOG(GBA_SIO, INFO, "[test32] setMode -> %i", mode);
}

static bool GBASIOTest32HandlesMode(struct GBASIODriver* driver, enum GBASIOMode mode) {
	UNUSED(driver);
	return mode == GBA_SIO_NORMAL_32;
}

static int GBASIOTest32ConnectedDevices(struct GBASIODriver* driver) {
	UNUSED(driver);
	return 1;
}

static int GBASIOTest32DeviceId(struct GBASIODriver* driver) {
	UNUSED(driver);
	return 0;
}

static uint16_t GBASIOTest32WriteSIOCNT(struct GBASIODriver* driver, uint16_t value) {
	UNUSED(driver);
	return value;
}

static bool GBASIOTest32Start(struct GBASIODriver* driver) {
	struct GBASIOTest32* test = (struct GBASIOTest32*) driver;
	struct GBA* gba = driver->p->p;
	uint32_t word;

	// IMMEDIATE REPLY: the GBA-side Normal32Transfer reads word N's answer on word N's own
	// transfer (no lag compensation), so FinishNormal32 returns the reply we compute below for
	// THIS word. (The one-transfer-lag emulation of the real Pico SPI master was removed: that
	// hardware path is moving to UART, and the lag desynced this ROM's session framing -- the GBA
	// bailed before draining the reply, leaving respRemaining>0 so the next OPEN/header/payload
	// words got eaten as reply polls, which leaked session words into forwarded frames.)
	test->haveReply = false; // default: this word's reply is open bus unless set below

	// Only NORMAL_32 is ours; ignore other modes entirely.
	if (driver->p->mode != GBA_SIO_NORMAL_32) {
		return true;
	}

	word = gba->memory.io[GBA_REG(SIODATA32_LO)] | (gba->memory.io[GBA_REG(SIODATA32_HI)] << 16);
	test->lastSent = word;

	if (!test->active) {
		// Dormant: the game's own NORMAL_32 traffic (link/RFU detection) gets open
		// bus and is never forwarded. Only our SESSION_OPEN handshake wakes us up.
		if (word == TEST32_SESSION_OPEN && _ensureConnected(test)) {
			test->active = true;
			test->frameRemaining = 0;
			test->respRemaining = 0;
			test->sendValue = TEST32_SESSION_OPEN; // ack the open
			test->haveReply = true;
			mLOG(GBA_SIO, INFO, "[test32] session opened");
		} else if (word == TEST32_SESSION_BYE) {
			// Player turned sync off: close the TCP connection so the server sees
			// us leave immediately (instead of waiting for a presence timeout).
			_dropConnection(test);
			mLOG(GBA_SIO, INFO, "[test32] connection closed (bye)");
		}
		return true;
	}

	// Feeding a buffered multi-word response back to the GBA: each transfer clocks
	// out the next response word; the GBA's outgoing words here are just polls.
	if (test->respRemaining > 0) {
		uint8_t* p = &test->respData[(test->respWords - test->respRemaining) * 4];
		test->sendValue = p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t) p[3] << 24);
		test->haveReply = true;
		test->respRemaining--;
		return true;
	}

	if (test->frameRemaining == 0) {
		// Header word: either SESSION_CLOSE, or a length prefix for the next frame.
		if (word == TEST32_SESSION_CLOSE) {
			test->active = false;
			test->sendValue = TEST32_SESSION_CLOSE; // ack the close
			test->haveReply = true;
			mLOG(GBA_SIO, INFO, "[test32] session closed");
			return true;
		}
		uint32_t n = word & 0xFF;
		if (n == 0 || n > TEST32_MAX_FRAME_WORDS) {
			test->sendValue = 0; // ack empty/invalid header, no frame
			test->haveReply = true;
			return true;
		}
		test->frameLen = n;
		test->frameRemaining = n;
		test->frameFill = 0;
		test->sendValue = 0; // ack header
		test->haveReply = true;
		return true;
	}

	// Payload word: accumulate. Any value is fine here -- the length prefix means
	// payload is consumed by count, never reinterpreted as a magic word.
	test->frameData[test->frameFill++] = word & 0xFF;
	test->frameData[test->frameFill++] = (word >> 8) & 0xFF;
	test->frameData[test->frameFill++] = (word >> 16) & 0xFF;
	test->frameData[test->frameFill++] = (word >> 24) & 0xFF;
	test->frameRemaining--;

	if (test->frameRemaining > 0) {
		test->sendValue = 0; // ack intermediate payload word
		test->haveReply = true;
		return true;
	}

	// Frame complete: forward the payload to the server. The response is variable
	// length, so this final payload transfer returns the response WORD COUNT; the
	// GBA then clocks that many words back via the respRemaining feeder above.
	{
		uint32_t contentLen = 0;
		// Length-prefix the frame so the server can reassemble it even if TCP splits or
		// coalesces packets over real WiFi/hardware (on localhost it always arrives whole).
		// Wire format: [lenHi][lenLo][payload...]; payload is the verbatim CelioServer
		// command ("PS_", "PU_", ...). Server responses are already framed (Message header).
		uint8_t framed[2 + sizeof(test->frameData)];
		size_t framedLen = (size_t) test->frameFill + 2;
		framed[0] = (uint8_t) ((test->frameFill >> 8) & 0xFF);
		framed[1] = (uint8_t) (test->frameFill & 0xFF);
		memcpy(&framed[2], test->frameData, test->frameFill);
		if (SocketSend(test->socket, framed, framedLen) != (ssize_t) framedLen) {
			mLOG(GBA_SIO, WARN, "[test32] frame send failed; closing session");
			_dropConnection(test);
			test->active = false;
		} else if (_recvCelioContent(test, test->respData, sizeof(test->respData), &contentLen)) {
			uint32_t words = (contentLen + 3) / 4;
			if (words > TEST32_MAX_RESP_WORDS) {
				words = TEST32_MAX_RESP_WORDS;
			}
			test->respWords = (uint8_t) words;
			test->respRemaining = (uint8_t) words;  // all words clocked out via the feeder
			test->sendValue = words;                // this transfer carries the count, not data
			test->haveReply = true;
			mLOG(GBA_SIO, INFO, "[test32] frame -> celio -> %u byte reply (%u words)", contentLen, words);
		} else {
			mLOG(GBA_SIO, WARN, "[test32] no celio response within %ims", TEST32_RECV_TIMEOUT);
		}
	}
	return true;
}

static uint32_t GBASIOTest32FinishNormal32(struct GBASIODriver* driver) {
	struct GBASIOTest32* test = (struct GBASIOTest32*) driver;
	uint32_t value = test->haveReply ? test->sendValue : TEST32_NO_DATA;
	mLOG(GBA_SIO, INFO, "[test32] finishNormal32 -> %08X", value);
	return value;
}
