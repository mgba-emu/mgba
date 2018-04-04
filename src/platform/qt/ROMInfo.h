/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QWidget>

#include <memory>

#include "ui_ROMInfo.h"

namespace QGBA {

class CoreController;

class ROMInfo : public QDialog {
Q_OBJECT

public:
	ROMInfo(std::shared_ptr<CoreController> controller, QWidget* parent = nullptr);

private:
	Ui::ROMInfo m_ui;
};

}
