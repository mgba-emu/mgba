/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GDB_STUB_H
#define GDB_STUB_H

#include "util/common.h"

#include "debugger/debugger.h"

#include "util/socket.h"

#define GDB_STUB_MAX_LINE 1200

enum GDBStubAckState {
	GDB_ACK_PENDING = 0,
	GDB_ACK_RECEIVED,
	GDB_NAK_RECEIVED,
	GDB_ACK_OFF
};

struct GDBStub {
	struct ARMDebugger d;

	char line[GDB_STUB_MAX_LINE];
	char outgoing[GDB_STUB_MAX_LINE];
	enum GDBStubAckState lineAck;

	Socket socket;
	Socket connection;
};

void GDBStubCreate(struct GDBStub*);
int GDBStubListen(struct GDBStub*, int port, uint32_t bindAddress);

void GDBStubHangup(struct GDBStub*);
void GDBStubShutdown(struct GDBStub*);

void GDBStubUpdate(struct GDBStub*);

#endif
