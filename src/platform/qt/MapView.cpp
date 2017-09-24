/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MapView.h"

#include "CoreController.h"
#include "GBAApp.h"
#include "LogController.h"

#include <mgba-util/png-io.h>

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
#ifdef USE_PNG
	connect(m_ui.exportButton, &QAbstractButton::clicked, this, &MapView::exportMap);
#else
	m_ui.exportButton->setVisible(false);
#endif
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
	{
		CoreController::Interrupter interrupter(m_controller);
		mMapCache* mapCache = mMapCacheSetGetPointer(&m_cacheSet->maps, m_map);
		int tilesW = 1 << mMapCacheSystemInfoGetTilesWide(mapCache->sysConfig);
		int tilesH = 1 << mMapCacheSystemInfoGetTilesHigh(mapCache->sysConfig);
		m_rawMap = QImage(QSize(tilesW * 8, tilesH * 8), QImage::Format_ARGB32);
		uchar* bgBits = m_rawMap.bits();
		for (int j = 0; j < tilesH; ++j) {
			for (int i = 0; i < tilesW; ++i) {
				mMapCacheCleanTile(mapCache, m_mapStatus, i, j);
			}
			for (int i = 0; i < 8; ++i) {
				memcpy(static_cast<void*>(&bgBits[tilesW * 32 * (i + j * 8)]), mMapCacheGetRow(mapCache, i + j * 8), tilesW * 32);
			}
		}
	}
	m_rawMap = m_rawMap.rgbSwapped();
	QPixmap map = QPixmap::fromImage(m_rawMap.convertToFormat(QImage::Format_RGB32));
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

#ifdef USE_PNG
void MapView::exportMap() {
	QString filename = GBAApp::app()->getSaveFileName(this, tr("Export map"),
	                                                  tr("Portable Network Graphics (*.png)"));
	VFile* vf = VFileDevice::open(filename, O_WRONLY | O_CREAT | O_TRUNC);
	if (!vf) {
		LOG(QT, ERROR) << tr("Failed to open output PNG file: %1").arg(filename);
		return;
	}

	CoreController::Interrupter interrupter(m_controller);
	png_structp png = PNGWriteOpen(vf);
	png_infop info = PNGWriteHeaderA(png, m_rawMap.width(), m_rawMap.height());

	mMapCache* mapCache = mMapCacheSetGetPointer(&m_cacheSet->maps, m_map);
	QImage map = m_rawMap.rgbSwapped();
	PNGWritePixelsA(png, map.width(), map.height(), map.bytesPerLine() / 4, static_cast<const void*>(map.constBits()));
	PNGWriteClose(png, info);
}
#endif
