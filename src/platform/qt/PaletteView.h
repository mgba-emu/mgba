/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_PALETTE_VIEW
#define QGBA_PALETTE_VIEW

#include <QWidget>

#include "GameController.h"
#include "Swatch.h"

#include "ui_PaletteView.h"

namespace QGBA {

class Swatch;

class PaletteView : public QWidget {
Q_OBJECT

public:
	PaletteView(GameController* controller, QWidget* parent = nullptr);

public slots:
	void updatePalette();

private slots:
	void selectIndex(int);

private:
	void exportPalette(int start, int length);

	Ui::PaletteView m_ui;

	GameController* m_controller;
};

}

#endif
