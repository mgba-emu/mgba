/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QTimer>
#include <QTransform>
#include <QWidget>

#include <mgba/core/cache-set.h>

#include <memory>

struct mMapCacheEntry;

namespace QGBA {

class CoreController;

class AssetView : public QWidget {
Q_OBJECT

public:
	AssetView(std::shared_ptr<CoreController> controller, QWidget* parent = nullptr);

protected slots:
	void updateTiles();
	void updateTiles(bool force);

protected:
	mCacheSet* const m_cacheSet;
	std::shared_ptr<CoreController> m_controller;

	struct ObjInfo {
		unsigned tile;
		unsigned width;
		unsigned height;
		unsigned stride;
		unsigned paletteId;
		unsigned paletteSet;
		unsigned bits;

		bool enabled : 1;
		unsigned priority : 2;
		int x : 10;
		int y : 10;
		bool hflip : 1;
		bool vflip : 1;
		QTransform xform;

		bool operator!=(const ObjInfo&) const;
	};

	static void compositeTile(const void* tile, void* image, size_t stride, size_t x, size_t y, int depth = 8);
	QImage compositeMap(int map, QVector<mMapCacheEntry>*);
	QImage compositeObj(const ObjInfo&);

	bool lookupObj(int id, struct ObjInfo*);

#ifdef M_CORE_GBA
	virtual void updateTilesGBA(bool force) = 0;
#endif
#ifdef M_CORE_GB
	virtual void updateTilesGB(bool force) = 0;
#endif

	void resizeEvent(QResizeEvent*) override;
	void showEvent(QShowEvent*) override;

private:
#ifdef M_CORE_GBA
	bool lookupObjGBA(int id, struct ObjInfo*);
#endif
#ifdef M_CORE_GB
	bool lookupObjGB(int id, struct ObjInfo*);
#endif

	QTimer m_updateTimer;
};

}
