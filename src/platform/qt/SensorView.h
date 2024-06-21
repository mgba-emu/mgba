/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QTimer>
#include <QDialog>

#include <functional>
#include <memory>

#include "ui_SensorView.h"

struct mRotationSource;

namespace QGBA {

class ConfigController;
class CoreController;
class GamepadAxisEvent;
class InputController;
class InputDriver;

class SensorView : public QDialog {
Q_OBJECT

public:
	SensorView(InputController* input, QWidget* parent = nullptr);

	void setController(std::shared_ptr<CoreController>);

protected:
	bool eventFilter(QObject*, QEvent* event) override;
	bool event(QEvent* event) override;

private slots:
	void updateSensors();
	void setLuminanceValue(int);
	void luminanceValueChanged(int);

private:
	Ui::SensorView m_ui;

	QAbstractButton* m_button = nullptr;
	void (InputDriver::*m_setter)(int);

	std::shared_ptr<CoreController> m_controller;
	InputController* m_input;
	mRotationSource* m_rotation;
	QTimer m_timer;

	void jiggerer(QAbstractButton*, void (InputDriver::*)(int));
};

}
