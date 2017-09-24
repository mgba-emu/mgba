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
#ifdef M_CORE_GBA
#include <mgba/internal/gba/memory.h>
#endif
#ifdef M_CORE_GB
#include <mgba/internal/gb/memory.h>
#endif

#include <QButtonGroup>
#include <QFontDatabase>
#include <QMouseEvent>
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
		m_boundary = 2048;
		m_addressBase = BASE_VRAM;
		m_addressWidth = 8;
		break;
#endif
#ifdef M_CORE_GB
	case PLATFORM_GB:
		m_boundary = 1024;
		m_addressBase = GB_BASE_VRAM;
		m_addressWidth = 4;
		break;
#endif
	default:
		return;
	}
	m_ui.tile->setBoundary(m_boundary, 0, 0);

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
	m_ui.map->installEventFilter(this);
	m_ui.tile->addCustomProperty("mapAddr", tr("Map Addr."));
	m_ui.tile->addCustomProperty("flip", tr("Mirror"));
	selectTile(0, 0);
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

void MapView::selectTile(int x, int y) {
	CoreController::Interrupter interrupter(m_controller);
	mMapCache* mapCache = mMapCacheSetGetPointer(&m_cacheSet->maps, m_map);
	size_t tileCache = mTileCacheSetIndex(&m_cacheSet->tiles, mapCache->tileCache);
	m_ui.tile->setBoundary(m_boundary, tileCache, tileCache);
	uint32_t location = mMapCacheTileId(mapCache, x, y);
	mMapCacheEntry* entry = &m_mapStatus[location];
	m_ui.tile->selectIndex(entry->tileId + mapCache->tileStart);
	m_ui.tile->setPalette(mMapCacheEntryFlagsGetPaletteId(entry->flags));
	m_ui.tile->setFlip(mMapCacheEntryFlagsGetHMirror(entry->flags), mMapCacheEntryFlagsGetVMirror(entry->flags));
	location <<= (mMapCacheSystemInfoGetMapAlign(mapCache->sysConfig));
	location += m_addressBase + mapCache->mapStart;

	QString flip(tr("None"));
	if (mMapCacheEntryFlagsGetHMirror(entry->flags) && mMapCacheEntryFlagsGetVMirror(entry->flags)) {
		flip = tr("Both");
	} else if (mMapCacheEntryFlagsGetHMirror(entry->flags)) {
		flip = tr("Horizontal");
	} else if (mMapCacheEntryFlagsGetVMirror(entry->flags)) {
		flip = tr("Vertical");
	}
	m_ui.tile->setCustomProperty("flip", flip);
	m_ui.tile->setCustomProperty("mapAddr", QString("%0%1")
		.arg(m_addressWidth == 8 ? "0x" : "")
		.arg(location, m_addressWidth, 16, QChar('0')));
}

bool MapView::eventFilter(QObject* obj, QEvent* event) {
	if (event->type() != QEvent::MouseButtonPress) {
		return false;
	}
	int x = static_cast<QMouseEvent*>(event)->x();
	int y = static_cast<QMouseEvent*>(event)->y();
	x /= 8 * m_ui.magnification->value();
	y /= 8 * m_ui.magnification->value();
	selectTile(x, y);
	return true;
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
