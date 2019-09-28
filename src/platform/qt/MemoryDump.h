/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "ui_MemoryDump.h"

#include <memory>

namespace QGBA {

class CoreController;

class MemoryDump : public QDialog {
Q_OBJECT

public:
	MemoryDump(std::shared_ptr<CoreController> controller, QWidget* parent = nullptr);

public slots:
	void setSegment(int);
	void setAddress(uint32_t address);
	void setByteCount(uint32_t);

private slots:
	void save();

private:
	QByteArray serialize();

	Ui::MemoryDump m_ui;

	std::shared_ptr<CoreController> m_controller;
};

}
