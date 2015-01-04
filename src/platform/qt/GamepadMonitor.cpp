/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GamepadMonitor.h"

#include "InputController.h"

#include <QTimer>

using namespace QGBA;

GamepadMonitor::GamepadMonitor(InputController* controller, QObject* parent)
	: QObject(parent)
	, m_controller(controller)
{
#ifdef BUILD_SDL
	m_gamepadTimer = new QTimer(this);
	connect(m_gamepadTimer, SIGNAL(timeout()), this, SLOT(testGamepad()));
	m_gamepadTimer->setInterval(50);
	m_gamepadTimer->start();
#endif
}

void GamepadMonitor::testGamepad() {
#ifdef BUILD_SDL
	m_gamepadTimer->setInterval(50);

	auto activeAxes = m_controller->activeGamepadAxes();
	auto oldAxes = m_activeAxes;
	m_activeAxes = activeAxes;
	activeAxes.subtract(oldAxes);
	if (!activeAxes.empty()) {
		emit axisChanged(activeAxes.begin()->first, activeAxes.begin()->second);
	}

	auto activeButtons = m_controller->activeGamepadButtons();
	auto oldButtons = m_activeButtons;
	m_activeButtons = activeButtons;
	activeButtons.subtract(oldButtons);
	if (!activeButtons.empty()) {
		emit buttonPressed(*activeButtons.begin());
	}
#endif
}
