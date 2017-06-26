/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_MEMORY_VIEW
#define QGBA_MEMORY_VIEW

#include "MemoryModel.h"

#include "ui_MemoryView.h"

namespace QGBA {

class GameController;

class MemoryView : public QWidget {
Q_OBJECT

public:
	MemoryView(GameController* controller, QWidget* parent = nullptr);

public slots:
	void update();
	void jumpToAddress(uint32_t address) { m_ui.hexfield->jumpToAddress(address); }

private slots:
	void setIndex(int);
	void setSegment(int);
	void updateSelection(uint32_t start, uint32_t end);
	void updateStatus();

private:
	Ui::MemoryView m_ui;

	GameController* m_controller;
	QPair<uint32_t, uint32_t> m_selection;
};

}

#endif
