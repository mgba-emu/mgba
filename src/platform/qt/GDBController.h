/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "DebuggerController.h"

#ifdef USE_GDB_STUB

#include <mgba/internal/debugger/gdb-stub.h>

namespace QGBA {

class CoreController;

class GDBController : public DebuggerController {
Q_OBJECT

public:
	GDBController(QObject* parent = nullptr);

public:
	ushort port();
	bool isAttached();

public slots:
	void setPort(ushort port);
	void setBindAddress(const Address&);
	void setWatchpointsBehavior(int watchpointsBehaviorId);
	void listen();

signals:
	void listening();
	void listenFailed();

private:
	virtual void shutdownInternal() override;

	GDBStub m_gdbStub{};

	ushort m_port = 2345;
	Address m_bindAddress;
	enum GDBWatchpointsBehvaior m_watchpointsBehavior = GDB_WATCHPOINT_STANDARD_LOGIC;
};

}

#endif
