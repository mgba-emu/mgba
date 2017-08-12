/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "ui_AssetTile.h"

#include <memory>

#include <mgba/core/tile-cache.h>

namespace QGBA {

class CoreController;

class AssetTile : public QGroupBox {
Q_OBJECT

public:
	AssetTile(QWidget* parent = nullptr);
	void setController(std::shared_ptr<CoreController>);

public slots:
	void setPalette(int);
	void setPaletteSet(int, int boundary, int max);
	void selectIndex(int);
	void selectColor(int);

private:
	Ui::AssetTile m_ui;

	mTileCache* m_tileCache;
	int m_paletteId = 0;
	int m_paletteSet = 0;
	int m_index = 0;

	int m_addressWidth;
	int m_addressBase;
	int m_boundary;
	int m_boundaryBase;
};

}
