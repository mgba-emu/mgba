/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "ui_AssetTile.h"

#include <memory>

#include <mgba/core/cache-set.h>

namespace QGBA {

class CoreController;

class AssetTile : public AssetInfo {
Q_OBJECT

public:
	AssetTile(QWidget* parent = nullptr);
	void setController(std::shared_ptr<CoreController>);
	QImage activeTile() const { return m_activeTile; }

public slots:
	void setPalette(int);
	void setBoundary(int boundary, int set0, int set1);
	void selectIndex(int);
	void setFlip(bool h, bool v);
	void selectColor(int);
	void setMaxTile(int);

protected:
	int customLocation(const QString& id = {}) override;

private:
	Ui::AssetTile m_ui;

	mCacheSet* m_cacheSet;
	mTileCache* m_tileCaches[2];
	int m_paletteId = 0;
	int m_index = 0;

	int m_addressWidth;
	int m_addressBase;
	int m_boundary;
	int m_boundaryBase;
	int m_maxTile;
	bool m_flipH = false;
	bool m_flipV = false;

	QMap<QString, QLabel*> m_customProperties;
	QImage m_activeTile;
};

}
