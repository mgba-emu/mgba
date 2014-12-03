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
	, m_bindAddress(0)
{
	GDBStubCreate(&m_gdbStub);
}

ushort GDBController::port() {
	return m_port;
}

uint32_t GDBController::bindAddress() {
	return m_bindAddress;
}

bool GDBController::isAttached() {
	return m_gameController->debugger() == &m_gdbStub.d;
}

void GDBController::setPort(ushort port) {
	m_port = port;
}

void GDBController::setBindAddress(uint32_t bindAddress) {
	m_bindAddress = bindAddress;
}

void GDBController::attach() {
	if (isAttached()) {
		return;
	}
	m_gameController->setDebugger(&m_gdbStub.d);
}

void GDBController::detach() {
	if (!isAttached()) {
		return;
	}
	bool wasPaused = m_gameController->isPaused();
	disconnect(m_gameController, SIGNAL(frameAvailable(const uint32_t*)), this, SLOT(updateGDB()));
	m_gameController->setPaused(true);
	GDBStubShutdown(&m_gdbStub);
	m_gameController->setDebugger(nullptr);
	m_gameController->setPaused(wasPaused);
}

void GDBController::listen() {
	if (!isAttached()) {
		attach();
	}
	bool wasPaused = m_gameController->isPaused();
	connect(m_gameController, SIGNAL(frameAvailable(const uint32_t*)), this, SLOT(updateGDB()));
	m_gameController->setPaused(true);
	GDBStubListen(&m_gdbStub, m_port, m_bindAddress);
	m_gameController->setPaused(wasPaused);
}

void GDBController::updateGDB() {
	bool wasPaused = m_gameController->isPaused();
	m_gameController->setPaused(true);
	GDBStubUpdate(&m_gdbStub);
	m_gameController->setPaused(wasPaused);
}
