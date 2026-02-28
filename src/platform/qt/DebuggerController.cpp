/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GDBController.h"

#include "CoreController.h"

using namespace QGBA;

DebuggerController::DebuggerController(mDebuggerModule* debugger, QObject* parent)
	: QObject(parent)
	, m_debugger(debugger)
{
}

bool DebuggerController::isAttached() {
	if (!m_controller) {
		return false;
	}
	return m_controller->debugger() == m_debugger->p;
}

void DebuggerController::onCoreDetached(std::shared_ptr<CoreController>) {
	detach();
}

void DebuggerController::onCoreAttached(std::shared_ptr<CoreController>) {
	if (m_autoattach) {
		m_autoattach = false;
		attach();
	}
}

void DebuggerController::attach() {
	if (isAttached()) {
		return;
	}
	if (m_controller) {
		attachInternal();
		m_controller->attachDebuggerModule(m_debugger);
	} else {
		m_autoattach = true;
	}
}

void DebuggerController::detach() {
	if (!isAttached()) {
		return;
	}
	if (m_controller) {
		CoreController::Interrupter interrupter(m_controller);
		shutdownInternal();
		m_controller->detachDebuggerModule(m_debugger);
	} else {
		m_autoattach = false;
	}
}

void DebuggerController::breakInto() {
	if (!isAttached()) {
		return;
	}
	CoreController::Interrupter interrupter(m_controller);
	mDebuggerEnter(m_debugger->p, DEBUGGER_ENTER_MANUAL, 0);
}

void DebuggerController::shutdown() {
	m_autoattach = false;
	if (!isAttached()) {
		return;
	}
	CoreController::Interrupter interrupter(m_controller);
	shutdownInternal();
}

void DebuggerController::attachInternal() {
	// No default implementation
}

void DebuggerController::shutdownInternal() {
	// No default implementation
}
