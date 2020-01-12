/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "MemoryModel.h"

#include "ui_MemoryView.h"

namespace QGBA {

class CoreController;

class MemoryView : public QWidget {
Q_OBJECT

public:
	MemoryView(std::shared_ptr<CoreController> controller, QWidget* parent = nullptr);

public slots:
	void update();
	void jumpToAddress(uint32_t address);

private slots:
	void setIndex(int);
	void setSegment(int);
	void updateSelection(uint32_t start, uint32_t end);
	void updateStatus();
	void saveRange();

private:
	Ui::MemoryView m_ui;

	std::shared_ptr<CoreController> m_controller;
	QPair<uint32_t, uint32_t> m_region;
	QPair<uint32_t, uint32_t> m_selection;
};

}
