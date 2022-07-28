/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GDBController.h"

#include "CoreController.h"

using namespace QGBA;

GDBController::GDBController(QObject* parent)
	: DebuggerController(&m_gdbStub.d, parent)
	, m_bindAddress({ IPV4, {0} })
{
	GDBStubCreate(&m_gdbStub);
}

ushort GDBController::port() {
	return m_port;
}

bool GDBController::isAttached() {
	return m_gameController && m_gameController->debugger() == &m_gdbStub.d;
}

void GDBController::setPort(ushort port) {
	m_port = port;
}

void GDBController::setBindAddress(const Address& address) {
	m_bindAddress = address;
}

void GDBController::setWatchpointsBehavior(int watchpointsBehaviorId) {
	m_watchpointsBehavior = static_cast<GDBWatchpointsBehvaior>(watchpointsBehaviorId);
}

void GDBController::listen() {
	if (GDBStubListen(&m_gdbStub, m_port, &m_bindAddress, m_watchpointsBehavior)) {
		if (!isAttached()) {
			attach();
		}
		emit listening();
	} else {
		detach();
		emit listenFailed();
	}
}

void GDBController::shutdownInternal() {
	GDBStubShutdown(&m_gdbStub);
}
