/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_TILE_VIEW
#define QGBA_TILE_VIEW

#include "AssetView.h"
#include "GameController.h"

#include "ui_TileView.h"

#include <mgba/core/tile-cache.h>

namespace QGBA {

class TileView : public AssetView {
Q_OBJECT

public:
	TileView(GameController* controller, QWidget* parent = nullptr);

public slots:
	void updatePalette(int);

private:
#ifdef M_CORE_GBA
	void updateTilesGBA(bool force) override;
#endif
#ifdef M_CORE_GB
	void updateTilesGB(bool force) override;
#endif

	Ui::TileView m_ui;

	GameController* m_controller;
	mTileCacheEntry m_tileStatus[3072 * 32] = {}; // TODO: Correct size
	int m_paletteId = 0;
};

}

#endif
