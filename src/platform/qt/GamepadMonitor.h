/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_GAMEPAD_MONITOR
#define QGBA_GAMEPAD_MONITOR

#include <QObject>
#include <QSet>

class QTimer;

namespace QGBA {

class InputController;

class GamepadMonitor : public QObject {
Q_OBJECT

public:
	GamepadMonitor(InputController* controller, QObject* parent = nullptr);

signals:
	void axisChanged(int axis, int32_t value);
	void buttonPressed(int button);

public slots:
	void testGamepad();

private:
	InputController* m_controller;
	QSet<int> m_activeButtons;
	QSet<QPair<int, int32_t>> m_activeAxes;
	QTimer* m_gamepadTimer;
};

}

#endif
