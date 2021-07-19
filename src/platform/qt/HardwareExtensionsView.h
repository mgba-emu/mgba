/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once



#include <QWidget>

#include "ui_HardwareExtensionsView.h"

namespace QGBA {

class ConfigController;

class HardwareExtensionsView : public QWidget {
Q_OBJECT

public:
	HardwareExtensionsView(ConfigController* controller, QWidget* parent = nullptr);
	~HardwareExtensionsView();

private:
	Ui::HardwareExtensionsView m_ui;
	ConfigController* m_controller;
	unsigned int enabledCounter;

};

}