/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GDBController.h"

#include "GameController.h"

using namespace QGBA;

DebuggerController::DebuggerController(GameController* controller, mDebugger* debugger, QObject* parent)
	: QObject(parent)
	, m_gameController(controller)
	, m_debugger(debugger)
{
}

bool DebuggerController::isAttached() {
	return m_gameController->debugger() == m_debugger;
}

void DebuggerController::attach() {
	if (isAttached()) {
		return;
	}
	if (m_gameController->isLoaded()) {
		m_gameController->setDebugger(m_debugger);
		mDebuggerEnter(m_debugger, DEBUGGER_ENTER_ATTACHED, 0);
	} else {
		QObject::disconnect(m_autoattach);
		m_autoattach = connect(m_gameController, SIGNAL(gameStarted(mCoreThread*, const QString&)), this, SLOT(attach()));
	}
}

void DebuggerController::detach() {
	QObject::disconnect(m_autoattach);
	if (!isAttached()) {
		return;
	}
	m_gameController->threadInterrupt();
	shutdownInternal();
	m_gameController->setDebugger(nullptr);
	m_gameController->threadContinue();
}

void DebuggerController::breakInto() {
	if (!isAttached()) {
		return;
	}
	m_gameController->threadInterrupt();
	mDebuggerEnter(m_debugger, DEBUGGER_ENTER_MANUAL, 0);
	m_gameController->threadContinue();
}

void DebuggerController::shutdown() {
	QObject::disconnect(m_autoattach);
	if (!isAttached()) {
		return;
	}
	m_gameController->threadInterrupt();
	shutdownInternal();
	m_gameController->threadContinue();
}

void DebuggerController::shutdownInternal() {
	// No default implementation
}
