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

#include "CorePointer.h"
#include "ui_SensorView.h"

struct mRotationSource;

namespace QGBA {

class CoreController;
class GamepadAxisEvent;
class InputController;
class InputDriver;

class SensorView : public QDialog, public CoreConsumer {
Q_OBJECT

public:
	SensorView(CorePointerSource* controller, InputController* input, QWidget* parent = nullptr);

protected:
	bool eventFilter(QObject*, QEvent* event) override;
	bool event(QEvent* event) override;

private slots:
	void updateSensors();
	void setLuminanceValue(int);
	void luminanceValueChanged(int);

private:
	void onCoreAttached(std::shared_ptr<CoreController>);

	Ui::SensorView m_ui;

	QAbstractButton* m_button = nullptr;
	void (InputDriver::*m_setter)(int);

	InputController* m_input;
	mRotationSource* m_rotation;
	QTimer m_timer;

	void jiggerer(QAbstractButton*, void (InputDriver::*)(int));
};

}
