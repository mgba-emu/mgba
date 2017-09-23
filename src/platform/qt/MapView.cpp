/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MapView.h"

#include "CoreController.h"
#include "GBAApp.h"

#include <QButtonGroup>
#include <QFontDatabase>
#include <QRadioButton>
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

	CoreController::Interrupter interrupter(m_controller);
	const mCoreChannelInfo* videoLayers;
	size_t nVideo = m_controller->thread()->core->listVideoLayers(m_controller->thread()->core, &videoLayers);
	QButtonGroup* group = new QButtonGroup(this);
	for (size_t i = 0; i < nVideo; ++i) {
		if (strncmp(videoLayers[i].internalName, "bg", 2) != 0) {
			continue;
		}
		QRadioButton* button = new QRadioButton(tr(videoLayers[i].visibleName));
		if (!i) {
			button->setChecked(true);
		}
		m_ui.bgLayout->addWidget(button);
		connect(button, &QAbstractButton::pressed, button, [this, i]() {
			selectMap(i);
		});
		group->addButton(button);
	}
}

void MapView::selectMap(int map) {
	if (map >= mMapCacheSetSize(&m_cacheSet->maps)) {
		return;
	}
	if (map == m_map) {
		return;
	}
	m_map = map;
	updateTiles(true);
}

void MapView::updateTilesGBA(bool force) {
	QImage bg;
	{
		CoreController::Interrupter interrupter(m_controller);
		mMapCache* mapCache = mMapCacheSetGetPointer(&m_cacheSet->maps, m_map);
		int tilesW = 1 << mMapCacheSystemInfoGetTilesWide(mapCache->sysConfig);
		int tilesH = 1 << mMapCacheSystemInfoGetTilesHigh(mapCache->sysConfig);
		bg = QImage(QSize(tilesW * 8, tilesH * 8), QImage::Format_ARGB32);
		uchar* bgBits = bg.bits();
		for (int j = 0; j < tilesH; ++j) {
			for (int i = 0; i < tilesW; ++i) {
				mMapCacheCleanTile(mapCache, &m_mapStatus[i + j * tilesW], i, j);
			}
			for (int i = 0; i < 8; ++i) {
				memcpy(static_cast<void*>(&bgBits[tilesW * 32 * (i + j * 8)]), mMapCacheGetRow(mapCache, i + j * 8), tilesW * 32);
			}
		}
	}
	bg = bg.convertToFormat(QImage::Format_RGB32).rgbSwapped();
	QPixmap map = QPixmap::fromImage(bg);
	if (m_ui.magnification->value() > 1) {
		map = map.scaled(map.size() * m_ui.magnification->value());
	}
	m_ui.map->setPixmap(map);
}

#ifdef M_CORE_GB
void MapView::updateTilesGB(bool force) {
	updateTilesGBA(force);
}
#endif

