/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_GDB_CONTROLLER
#define QGBA_GDB_CONTROLLER

#include <QObject>

#ifdef USE_GDB_STUB

extern "C" {
#include "debugger/gdb-stub.h"
}

namespace QGBA {

class GameController;

class GDBController : public QObject {
Q_OBJECT

public:
	GDBController(GameController* controller, QObject* parent = nullptr);

public:
	ushort port();
	bool isAttached();

public slots:
	void setPort(ushort port);
	void setBindAddress(uint32_t bindAddress);
	void attach();
	void detach();
	void listen();

signals:
	void listening();
	void listenFailed();

private:
	GDBStub m_gdbStub;
	GameController* m_gameController;

	ushort m_port;
	Address m_bindAddress;
};

}

#endif

#endif
