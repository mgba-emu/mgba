/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_GDB_CONTROLLER
#define QGBA_GDB_CONTROLLER

#include "DebuggerController.h"

#ifdef USE_GDB_STUB

#include <mgba/internal/debugger/gdb-stub.h>

namespace QGBA {

class GameController;

class GDBController : public DebuggerController {
Q_OBJECT

public:
	GDBController(GameController* controller, QObject* parent = nullptr);

public:
	ushort port();
	bool isAttached();

public slots:
	void setPort(ushort port);
	void setBindAddress(uint32_t bindAddress);
	void listen();

signals:
	void listening();
	void listenFailed();

private:
	virtual void shutdownInternal() override;

	GDBStub m_gdbStub;

	ushort m_port = 2345;
	Address m_bindAddress;
};

}

#endif

#endif
