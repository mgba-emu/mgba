/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_SENSOR_VIEW
#define QGBA_SENSOR_VIEW

#include <QTimer>
#include <QDialog>

#include <functional>

#include "ui_SensorView.h"

struct mRotationSource;

namespace QGBA {

class ConfigController;
class GameController;
class GamepadAxisEvent;
class InputController;

class SensorView : public QDialog {
Q_OBJECT

public:
	SensorView(GameController* controller, InputController* input, QWidget* parent = nullptr);

protected:
	bool eventFilter(QObject*, QEvent* event) override;
	bool event(QEvent* event) override;

private slots:
	void updateSensors();
	void setLuminanceValue(int);
	void luminanceValueChanged(int);

private:
	Ui::SensorView m_ui;

	std::function<void(int)> m_jiggered;
	GameController* m_controller;
	InputController* m_input;
	mRotationSource* m_rotation;
	QTimer m_timer;

	void jiggerer(QAbstractButton*, void (InputController::*)(int));
};

}

#endif
