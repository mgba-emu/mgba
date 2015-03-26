/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_SENSOR_VIEW
#define QGBA_SENSOR_VIEW

#include <QWidget>

#include "ui_SensorView.h"

namespace QGBA {

class ConfigController;
class GameController;

class SensorView : public QWidget {
Q_OBJECT

public:
	SensorView(GameController* controller, QWidget* parent = nullptr);

private slots:
	void setLuminanceValue(int);
	void luminanceValueChanged(int);

private:
	Ui::SensorView m_ui;

	GameController* m_controller;
};

}

#endif
