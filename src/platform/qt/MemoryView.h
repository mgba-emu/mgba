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

private slots:
	void setIndex(int);
	void updateStatus(uint32_t start, uint32_t end);

private:
	Ui::MemoryView m_ui;

	GameController* m_controller;
};

}

#endif
