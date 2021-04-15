/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "ui_DolphinConnector.h"

#include <memory>

namespace QGBA {

class CoreController;
class Window;

class DolphinConnector : public QDialog {
Q_OBJECT

public:
	DolphinConnector(Window* window, QWidget* parent = nullptr);

public slots:
	void attach();
	void detach();

private slots:
	void updateAttached();

private:
	Ui::DolphinConnector m_ui;

	std::shared_ptr<CoreController> m_controller;
	Window* m_window;
};

}
