/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GDBController.h"

#include "GameController.h"

using namespace QGBA;

DebuggerController::DebuggerController(GameController* controller, mDebugger* debugger, QObject* parent)
	: QObject(parent)
	, m_debugger(debugger)
	, m_gameController(controller)
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
		attachInternal();
		m_gameController->setDebugger(m_debugger);
		mDebuggerEnter(m_debugger, DEBUGGER_ENTER_ATTACHED, 0);
	} else {
		QObject::disconnect(m_autoattach);
		m_autoattach = connect(m_gameController, &GameController::gameStarted, this, &DebuggerController::attach);
	}
}

void DebuggerController::detach() {
	QObject::disconnect(m_autoattach);
	if (!isAttached()) {
		return;
	}
	GameController::Interrupter interrupter(m_gameController);
	shutdownInternal();
	m_gameController->setDebugger(nullptr);
}

void DebuggerController::breakInto() {
	if (!isAttached()) {
		return;
	}
	GameController::Interrupter interrupter(m_gameController);
	mDebuggerEnter(m_debugger, DEBUGGER_ENTER_MANUAL, 0);
}

void DebuggerController::shutdown() {
	QObject::disconnect(m_autoattach);
	if (!isAttached()) {
		return;
	}
	GameController::Interrupter interrupter(m_gameController);
	shutdownInternal();
}

void DebuggerController::attachInternal() {
	// No default implementation
}

void DebuggerController::shutdownInternal() {
	// No default implementation
}
