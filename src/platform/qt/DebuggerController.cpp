/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GDBController.h"

#include "CoreController.h"

using namespace QGBA;

DebuggerController::DebuggerController(mDebugger* debugger, QObject* parent)
	: QObject(parent)
	, m_debugger(debugger)
{
}

bool DebuggerController::isAttached() {
	if (!m_gameController) {
		return false;
	}
	return m_gameController->debugger() == m_debugger;
}

void DebuggerController::setController(std::shared_ptr<CoreController> controller) {
	if (m_gameController && controller != m_gameController) {
		m_gameController->disconnect(this);
		detach();
	}
	m_gameController = controller;
	if (controller) {
		connect(m_gameController.get(), &CoreController::stopping, [this]() {
			setController(nullptr);
		});
		if (m_autoattach) {
			m_autoattach = false;
			attach();
		}
	}
}

void DebuggerController::attach() {
	if (isAttached()) {
		return;
	}
	if (m_gameController) {
		attachInternal();
		m_gameController->setDebugger(m_debugger);
	} else {
		m_autoattach = true;
	}
}

void DebuggerController::detach() {
	if (!isAttached()) {
		return;
	}
	if (m_gameController) {
		CoreController::Interrupter interrupter(m_gameController);
		shutdownInternal();
		m_gameController->setDebugger(nullptr);
	} else {
		m_autoattach = false;
	}
}

void DebuggerController::breakInto() {
	if (!isAttached()) {
		return;
	}
	CoreController::Interrupter interrupter(m_gameController);
	mDebuggerEnter(m_debugger, DEBUGGER_ENTER_MANUAL, 0);
}

void DebuggerController::shutdown() {
	m_autoattach = false;
	if (!isAttached()) {
		return;
	}
	CoreController::Interrupter interrupter(m_gameController);
	shutdownInternal();
}

void DebuggerController::attachInternal() {
	// No default implementation
}

void DebuggerController::shutdownInternal() {
	// No default implementation
}
