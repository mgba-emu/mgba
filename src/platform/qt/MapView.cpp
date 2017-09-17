/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MapView.h"

#include "CoreController.h"
#include "GBAApp.h"

#include <QFontDatabase>
#include <QTimer>

using namespace QGBA;

MapView::MapView(std::shared_ptr<CoreController> controller, QWidget* parent)
	: AssetView(controller, parent)
	, m_controller(controller)
{
	m_ui.setupUi(this);
	m_ui.tile->setController(controller);

	switch (m_controller->platform()) {
#ifdef M_CORE_GBA
	case PLATFORM_GBA:
		m_ui.tile->setBoundary(2048, 0, 2);
		break;
#endif
#ifdef M_CORE_GB
	case PLATFORM_GB:
		m_ui.tile->setBoundary(1024, 0, 0);
		break;
#endif
	default:
		return;
	}

	connect(m_ui.magnification, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this]() {
		updateTiles(true);
	});
}

#ifdef M_CORE_GBA
void MapView::updateTilesGBA(bool force) {
	QImage bg(QSize(256, 256), QImage::Format_ARGB32);
	uchar* bgBits = bg.bits();
	mMapCache* mapCache = mMapCacheSetGetPointer(&m_cacheSet->maps, 0);
	for (int j = 0; j < 32; ++j) {
		for (int i = 0; i < 32; ++i) {
			mMapCacheCleanTile(mapCache, &m_mapStatus[i + j * 32], i, j);
		}
		for (int i = 0; i < 8; ++i) {
			memcpy(static_cast<void*>(&bgBits[256 * 4 * (i + j * 8)]), mMapCacheGetRow(mapCache, i + j * 8), 256 * 4);
		}
	}
	bg = bg.convertToFormat(QImage::Format_RGB32).rgbSwapped();
	m_ui.map->setPixmap(QPixmap::fromImage(bg));
}
#endif

#ifdef M_CORE_GB
void MapView::updateTilesGB(bool force) {
}
#endif

