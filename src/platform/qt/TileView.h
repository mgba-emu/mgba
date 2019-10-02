/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "AssetView.h"

#include "ui_TileView.h"

#include <mgba/core/tile-cache.h>

namespace QGBA {

class CoreController;

class TileView : public AssetView {
Q_OBJECT

public:
	TileView(std::shared_ptr<CoreController> controller, QWidget* parent = nullptr);

public slots:
	void updatePalette(int);
	void exportTiles();
	void exportTile();
	void copyTiles();
	void copyTile();

private:
#ifdef M_CORE_GBA
	void updateTilesGBA(bool force) override;
#endif
#ifdef M_CORE_GB
	void updateTilesGB(bool force) override;
#endif

	Ui::TileView m_ui;

	std::shared_ptr<CoreController> m_controller;
	mTileCacheEntry m_tileStatus[3072 * 32] = {}; // TODO: Correct size
	int m_paletteId = 0;
};

}
