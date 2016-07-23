/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_TILE_VIEW
#define QGBA_TILE_VIEW

#include <QWidget>

#include "GameController.h"

#include "ui_TileView.h"

extern "C" {
#include "gba/renderers/tile-cache.h"
}

namespace QGBA {

class TileView : public QWidget {
Q_OBJECT

public:
	TileView(GameController* controller, QWidget* parent = nullptr);
	virtual ~TileView();

public slots:
	void updateTiles(bool force = false);
	void updatePalette(int);

private slots:
	void selectIndex(int);

protected:
	void resizeEvent(QResizeEvent*) override;
	void showEvent(QShowEvent*) override;

private:
	Ui::TileView m_ui;

	GameController* m_controller;
	GBAVideoTileCache m_tileCache;
	int m_paletteId;
};

}

#endif
