/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_MEMORY_SEARCH
#define QGBA_MEMORY_SEARCH

#include "ui_MemorySearch.h"

namespace QGBA {

class GameController;

class MemorySearch : public QWidget {
Q_OBJECT

public:
	MemorySearch(GameController* controller, QWidget* parent = nullptr);

public slots:
	void search();

private:
	Ui::MemorySearch m_ui;

	GameController* m_controller;
};

}

#endif
