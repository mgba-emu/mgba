/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QTimer>
#include <QWidget>

#include <mgba/core/cache-set.h>

#include <memory>

namespace QGBA {

class CoreController;

class AssetView : public QWidget {
Q_OBJECT

public:
	AssetView(std::shared_ptr<CoreController> controller, QWidget* parent = nullptr);

	static void compositeTile(const void* tile, void* image, size_t stride, size_t x, size_t y, int depth = 8);

protected slots:
	void updateTiles();
	void updateTiles(bool force);

protected:
#ifdef M_CORE_GBA
	virtual void updateTilesGBA(bool force) = 0;
#endif
#ifdef M_CORE_GB
	virtual void updateTilesGB(bool force) = 0;
#endif

	void resizeEvent(QResizeEvent*) override;
	void showEvent(QShowEvent*) override;

	mCacheSet* const m_cacheSet;

private:
	std::shared_ptr<CoreController> m_controller;
	QTimer m_updateTimer;
};

}
