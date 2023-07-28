/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MapView.h"

#include "CoreController.h"
#include "GBAApp.h"
#include "LogController.h"

#include <mgba-util/image/png-io.h>
#include <mgba-util/vfs.h>
#ifdef M_CORE_GBA
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/memory.h>
#include <mgba/internal/gba/video.h>
#endif
#ifdef M_CORE_GB
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/memory.h>
#endif

#include <QAction>
#include <QButtonGroup>
#include <QClipboard>
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
	case mPLATFORM_GBA:
		m_boundary = 2048;
		m_ui.tile->setMaxTile(3072);
		m_addressBase = GBA_BASE_VRAM;
		m_addressWidth = 8;
		m_ui.bgInfo->addCustomProperty("priority", tr("Priority"));
		m_ui.bgInfo->addCustomProperty("screenBase", tr("Map base"));
		m_ui.bgInfo->addCustomProperty("charBase", tr("Tile base"));
		m_ui.bgInfo->addCustomProperty("size", tr("Size"));
		m_ui.bgInfo->addCustomProperty("offset", tr("Offset"));
		m_ui.bgInfo->addCustomProperty("transform", tr("Xform"));
		break;
#endif
#ifdef M_CORE_GB
	case mPLATFORM_GB:
		m_boundary = 1024;
		m_ui.tile->setMaxTile(512);
		m_addressBase = GB_BASE_VRAM;
		m_addressWidth = 4;
		m_ui.bgInfo->addCustomProperty("screenBase", tr("Map base"));
		m_ui.bgInfo->addCustomProperty("charBase", tr("Tile base"));
		m_ui.bgInfo->addCustomProperty("offset", tr("Offset"));
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
	connect(m_ui.exportButton, &QAbstractButton::clicked, this, &MapView::exportMap);
	connect(m_ui.copyButton, &QAbstractButton::clicked, this, &MapView::copyMap);

	QAction* exportAction = new QAction(this);
	exportAction->setShortcut(QKeySequence::Save);
	connect(exportAction, &QAction::triggered, this, &MapView::exportMap);
	addAction(exportAction);

	QAction* copyAction = new QAction(this);
	copyAction->setShortcut(QKeySequence::Copy);
	connect(copyAction, &QAction::triggered, this, &MapView::copyMap);
	addAction(copyAction);

	m_ui.map->installEventFilter(this);
	m_ui.tile->addCustomProperty("mapAddr", tr("Map Addr."));
	m_ui.tile->addCustomProperty("flip", tr("Mirror"));
	selectTile(0, 0);
}

void MapView::selectMap(int map) {
	if (map == m_map || map < 0) {
		return;
	}
	if (static_cast<unsigned>(map) >= mMapCacheSetSize(&m_cacheSet->maps)) {
		return;
	}
	m_map = map;
	m_mapStatus.fill({});
	// Different maps can have different max palette counts; set it to
	// 0 immediately to avoid tile lookups with state palette IDs break
	m_ui.tile->setPalette(0);
	updateTiles(true);
}

void MapView::selectTile(int x, int y) {
	CoreController::Interrupter interrupter(m_controller);
	mMapCache* mapCache = mMapCacheSetGetPointer(&m_cacheSet->maps, m_map);
	int tiles = mMapCacheTileCount(mapCache);
	if (m_mapStatus.size() != tiles) {
		m_mapStatus.resize(tiles);
		m_mapStatus.fill({});
	}
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

bool MapView::eventFilter(QObject*, QEvent* event) {
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

void MapView::updateTilesGBA(bool) {
	{
		CoreController::Interrupter interrupter(m_controller);
		int bitmap = -1;
		int priority = -1;
		int frame = 0;
		QString offset(tr("N/A"));
		QString transform(tr("N/A"));
#ifdef M_CORE_GBA
		if (m_controller->platform() == mPLATFORM_GBA) {
			uint16_t* io = static_cast<GBA*>(m_controller->thread()->core->board)->memory.io;
			int mode = GBARegisterDISPCNTGetMode(io[REG_DISPCNT >> 1]);
			if (m_map == 2 && mode > 2) {
				bitmap = mode == 4 ? 1 : 0;
				if (mode != 3) {
					frame = GBARegisterDISPCNTGetFrameSelect(io[REG_DISPCNT >> 1]);
				}
			}
			m_boundary = 1024;
			m_ui.tile->setMaxTile(1536);
			priority = GBARegisterBGCNTGetPriority(io[(REG_BG0CNT >> 1) + m_map]);
			if (mode == 0 || (mode == 1 && m_map != 2)) {
				offset = QString("%1, %2")
					.arg(io[(REG_BG0HOFS >> 1) + (m_map << 1)])
					.arg(io[(REG_BG0VOFS >> 1) + (m_map << 1)]);

				if (!GBARegisterBGCNTIs256Color(io[(REG_BG0CNT >> 1) + m_map])) {
					m_boundary = 2048;
					m_ui.tile->setMaxTile(3072);
				}
			} else if ((mode > 0 && m_map == 2) || (mode == 2 && m_map == 3)) {
				int32_t refX = io[(REG_BG2X_LO >> 1) + ((m_map - 2) << 2)];
				refX |= io[(REG_BG2X_HI >> 1) + ((m_map - 2) << 2)] << 16;
				int32_t refY = io[(REG_BG2Y_LO >> 1) + ((m_map - 2) << 2)];
				refY |= io[(REG_BG2Y_HI >> 1) + ((m_map - 2) << 2)] << 16;
				refX <<= 4;
				refY <<= 4;
				refX >>= 4;
				refY >>= 4;
				offset = QString("%1\n%2").arg(refX / 65536., 0, 'f', 3).arg(refY / 65536., 0, 'f', 3);
				transform = QString("%1 %2\n%3 %4")
					.arg(io[(REG_BG2PA >> 1) + ((m_map - 2) << 2)] / 256., 3, 'f', 2)
					.arg(io[(REG_BG2PB >> 1) + ((m_map - 2) << 2)] / 256., 3, 'f', 2)
					.arg(io[(REG_BG2PC >> 1) + ((m_map - 2) << 2)] / 256., 3, 'f', 2)
					.arg(io[(REG_BG2PD >> 1) + ((m_map - 2) << 2)] / 256., 3, 'f', 2);

			}
		}
#endif
#ifdef M_CORE_GB
		if (m_controller->platform() == mPLATFORM_GB) {
			uint8_t* io = static_cast<GB*>(m_controller->thread()->core->board)->memory.io;
			int x = io[m_map == 0 ? 0x42 : 0x4A];
			int y = io[m_map == 0 ? 0x43 : 0x4B];
			offset = QString("%1, %2").arg(x).arg(y);
		}
#endif
		if (bitmap >= 0) {
			mBitmapCache* bitmapCache = mBitmapCacheSetGetPointer(&m_cacheSet->bitmaps, bitmap);
			int width = mBitmapCacheSystemInfoGetWidth(bitmapCache->sysConfig);
			int height = mBitmapCacheSystemInfoGetHeight(bitmapCache->sysConfig);
			m_ui.bgInfo->setCustomProperty("screenBase", QString("0x%1").arg(m_addressBase + bitmapCache->bitsStart[frame], 8, 16, QChar('0')));
			m_ui.bgInfo->setCustomProperty("charBase", tr("N/A"));
			m_ui.bgInfo->setCustomProperty("size", QString("%1×%2").arg(width).arg(height));
			m_ui.bgInfo->setCustomProperty("priority", priority);
			m_ui.bgInfo->setCustomProperty("offset", offset);
			m_ui.bgInfo->setCustomProperty("transform", transform);
			m_rawMap = QImage(QSize(width, height), QImage::Format_ARGB32);
			uchar* bgBits = m_rawMap.bits();
			for (int j = 0; j < height; ++j) {
				mBitmapCacheCleanRow(bitmapCache, m_bitmapStatus.data(), j);
				memcpy(static_cast<void*>(&bgBits[width * j * 4]), mBitmapCacheGetRow(bitmapCache, j), width * 4);
			}
			m_rawMap = m_rawMap.convertToFormat(QImage::Format_RGB32).rgbSwapped();
		} else {
			mMapCache* mapCache = mMapCacheSetGetPointer(&m_cacheSet->maps, m_map);
			int tilesW = 1 << mMapCacheSystemInfoGetTilesWide(mapCache->sysConfig);
			int tilesH = 1 << mMapCacheSystemInfoGetTilesHigh(mapCache->sysConfig);
			m_ui.bgInfo->setCustomProperty("screenBase", QString("%0%1")
					.arg(m_addressWidth == 8 ? "0x" : "")
					.arg(m_addressBase + mapCache->mapStart, m_addressWidth, 16, QChar('0')));
			m_ui.bgInfo->setCustomProperty("charBase", QString("%0%1")
					.arg(m_addressWidth == 8 ? "0x" : "")
					.arg(m_addressBase + mapCache->tileCache->tileBase, m_addressWidth, 16, QChar('0')));
			m_ui.bgInfo->setCustomProperty("size", QString("%1×%2").arg(tilesW * 8).arg(tilesH * 8));
			m_ui.bgInfo->setCustomProperty("priority", priority);
			m_ui.bgInfo->setCustomProperty("offset", offset);
			m_ui.bgInfo->setCustomProperty("transform", transform);
			m_rawMap = compositeMap(m_map, &m_mapStatus);
		}
	}
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

void MapView::exportMap() {
	QString filename = GBAApp::app()->getSaveFileName(this, tr("Export map"),
	                                                  tr("Portable Network Graphics (*.png)"));
	if (filename.isNull()) {
		return;
	}

	CoreController::Interrupter interrupter(m_controller);
	m_rawMap.save(filename, "PNG");
}

void MapView::copyMap() {
	CoreController::Interrupter interrupter(m_controller);
	GBAApp::app()->clipboard()->setImage(m_rawMap);
}
