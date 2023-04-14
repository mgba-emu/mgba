/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GDB_STUB_H
#define GDB_STUB_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/debugger/debugger.h>

#include <mgba-util/socket.h>

#define GDB_STUB_MAX_LINE 1400
#define GDB_STUB_INTERVAL 32

enum GDBStubAckState {
	GDB_ACK_PENDING = 0,
	GDB_ACK_RECEIVED,
	GDB_NAK_RECEIVED,
	GDB_ACK_OFF
};

enum GDBWatchpointsBehvaior {
	GDB_WATCHPOINT_STANDARD_LOGIC = 0,
	GDB_WATCHPOINT_OVERRIDE_LOGIC,
	GDB_WATCHPOINT_OVERRIDE_LOGIC_ANY_WRITE,
};

struct GDBStub {
	struct mDebugger d;

	char line[GDB_STUB_MAX_LINE];
	char outgoing[GDB_STUB_MAX_LINE];
	char memoryMapXml[GDB_STUB_MAX_LINE];
	enum GDBStubAckState lineAck;

	Socket socket;
	Socket connection;

	bool shouldBlock;
	int untilPoll;

	bool supportsSwbreak;
	bool supportsHwbreak;

	enum GDBWatchpointsBehvaior watchpointsBehavior;
};

void GDBStubCreate(struct GDBStub*);
bool GDBStubListen(struct GDBStub*, int port, const struct Address* bindAddress, enum GDBWatchpointsBehvaior watchpointsBehavior);

void GDBStubHangup(struct GDBStub*);
void GDBStubShutdown(struct GDBStub*);

void GDBStubUpdate(struct GDBStub*);

CXX_GUARD_END

#endif
