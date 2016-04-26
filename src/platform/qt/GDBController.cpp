/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GDBController.h"

#include "GameController.h"

using namespace QGBA;

GDBController::GDBController(GameController* controller, QObject* parent)
	: QObject(parent)
	, m_gameController(controller)
	, m_port(2345)
	, m_bindAddress({ IPV4, 0 })
{
	GDBStubCreate(&m_gdbStub);
}

ushort GDBController::port() {
	return m_port;
}

bool GDBController::isAttached() {
	return m_gameController->debugger() == &m_gdbStub.d;
}

void GDBController::setPort(ushort port) {
	m_port = port;
}

void GDBController::setBindAddress(uint32_t bindAddress) {
	m_bindAddress.version = IPV4;
	m_bindAddress.ipv4 = htonl(bindAddress);
}

void GDBController::attach() {
	if (isAttached() || m_gameController->platform() != PLATFORM_GBA) {
		return;
	}
	m_gameController->setDebugger(&m_gdbStub.d);
	if (m_gameController->isLoaded()) {
		mDebuggerEnter(&m_gdbStub.d, DEBUGGER_ENTER_ATTACHED, 0);
	} else {
		QObject::disconnect(m_autoattach);
		m_autoattach = connect(m_gameController, &GameController::gameStarted, [this]() {
			QObject::disconnect(m_autoattach);
			mDebuggerEnter(&m_gdbStub.d, DEBUGGER_ENTER_ATTACHED, 0);
		});
	}
}

void GDBController::detach() {
	QObject::disconnect(m_autoattach);
	if (!isAttached()) {
		return;
	}
	m_gameController->threadInterrupt();
	GDBStubShutdown(&m_gdbStub);
	m_gameController->setDebugger(nullptr);
	m_gameController->threadContinue();
}

void GDBController::listen() {
	m_gameController->threadInterrupt();
	if (!isAttached()) {
		attach();
	}
	if (GDBStubListen(&m_gdbStub, m_port, &m_bindAddress)) {
		emit listening();
	} else {
		detach();
		emit listenFailed();
	}
	m_gameController->threadContinue();
}

void GDBController::breakInto() {
	if (!isAttached()) {
		return;
	}
	m_gameController->threadInterrupt();
	mDebuggerEnter(&m_gdbStub.d, DEBUGGER_ENTER_MANUAL, 0);
	m_gameController->threadContinue();
}
