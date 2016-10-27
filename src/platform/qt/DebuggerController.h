/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_DEBUGGER_CONTROLLER
#define QGBA_DEBUGGER_CONTROLLER

#include <QObject>

struct mDebugger;

namespace QGBA {

class GameController;

class DebuggerController : public QObject {
Q_OBJECT

public:
	DebuggerController(GameController* controller, mDebugger* debugger, QObject* parent = nullptr);

public:
	bool isAttached();

public slots:
	virtual void attach();
	virtual void detach();
	virtual void breakInto();
	virtual void shutdown();

protected:
	virtual void attachInternal();
	virtual void shutdownInternal();

	mDebugger* const m_debugger;
	GameController* const m_gameController;

private:
	QMetaObject::Connection m_autoattach;
};

}

#endif
