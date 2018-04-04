/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "TileView.h"

#include "CoreController.h"
#include "GBAApp.h"

#include <QFontDatabase>
#include <QTimer>

#ifdef M_CORE_GB
#include <mgba/internal/gb/gb.h>
#endif

using namespace QGBA;

TileView::TileView(std::shared_ptr<CoreController> controller, QWidget* parent)
	: AssetView(controller, parent)
	, m_controller(controller)
{
	m_ui.setupUi(this);
	m_ui.tile->setController(controller);

	connect(m_ui.tiles, &TilePainter::indexPressed, m_ui.tile, &AssetTile::selectIndex);
	connect(m_ui.paletteId, &QAbstractSlider::valueChanged, this, &TileView::updatePalette);

	switch (m_controller->platform()) {
#ifdef M_CORE_GBA
	case PLATFORM_GBA:
		m_ui.tile->setBoundary(2048, 0, 2);
		break;
#endif
#ifdef M_CORE_GB
	case PLATFORM_GB:
		m_ui.palette256->setEnabled(false);
		m_ui.tile->setBoundary(1024, 0, 0);
		break;
#endif
	default:
		return;
	}

	connect(m_ui.palette256, &QAbstractButton::toggled, [this](bool selected) {
		if (selected) {
			m_ui.paletteId->setValue(0);
		}
		switch (m_controller->platform()) {
#ifdef M_CORE_GBA
		case PLATFORM_GBA:
			m_ui.tile->setBoundary(2048 >> selected, selected, selected + 2);
			break;
#endif
#ifdef M_CORE_GB
		case PLATFORM_GB:
			return;
#endif
		default:
			break;
		}
		updateTiles(true);
	});
	connect(m_ui.magnification, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this]() {
		updateTiles(true);
	});
}

#ifdef M_CORE_GBA
void TileView::updateTilesGBA(bool force) {
	if (m_ui.palette256->isChecked()) {
		m_ui.tiles->setTileCount(1536);
		mTileCache* cache = mTileCacheSetGetPointer(&m_cacheSet->tiles, 1);
		for (int i = 0; i < 1024; ++i) {
			const color_t* data = mTileCacheGetTileIfDirty(cache, &m_tileStatus[16 * i], i, 0);
			if (data) {
				m_ui.tiles->setTile(i, data);
			} else if (force) {
				m_ui.tiles->setTile(i, mTileCacheGetTile(cache, i, 0));
			}
		}
		cache = mTileCacheSetGetPointer(&m_cacheSet->tiles, 3);
		for (int i = 1024; i < 1536; ++i) {
			const color_t* data = mTileCacheGetTileIfDirty(cache, &m_tileStatus[16 * i], i - 1024, 0);
			if (data) {
				m_ui.tiles->setTile(i, data);
			} else if (force) {
				m_ui.tiles->setTile(i, mTileCacheGetTile(cache, i - 1024, 0));
			}
		}
	} else {
		mTileCache* cache = mTileCacheSetGetPointer(&m_cacheSet->tiles, 0);
		m_ui.tiles->setTileCount(3072);
		for (int i = 0; i < 2048; ++i) {
			const color_t* data = mTileCacheGetTileIfDirty(cache, &m_tileStatus[16 * i], i, m_paletteId);
			if (data) {
				m_ui.tiles->setTile(i, data);
			} else if (force) {
				m_ui.tiles->setTile(i, mTileCacheGetTile(cache, i, m_paletteId));
			}
		}
		cache = mTileCacheSetGetPointer(&m_cacheSet->tiles, 2);
		for (int i = 2048; i < 3072; ++i) {
			const color_t* data = mTileCacheGetTileIfDirty(cache, &m_tileStatus[16 * i], i - 2048, m_paletteId);
			if (data) {
				m_ui.tiles->setTile(i, data);
			} else if (force) {
				m_ui.tiles->setTile(i, mTileCacheGetTile(cache, i - 2048, m_paletteId));
			}
		}
	}
}
#endif

#ifdef M_CORE_GB
void TileView::updateTilesGB(bool force) {
	const GB* gb = static_cast<const GB*>(m_controller->thread()->core->board);
	int count = gb->model >= GB_MODEL_CGB ? 1024 : 512;
	m_ui.tiles->setTileCount(count);
	mTileCache* cache = mTileCacheSetGetPointer(&m_cacheSet->tiles, 0);
	for (int i = 0; i < count; ++i) {
		const color_t* data = mTileCacheGetTileIfDirty(cache, &m_tileStatus[8 * i], i, m_paletteId);
		if (data) {
			m_ui.tiles->setTile(i, data);
		} else if (force) {
			m_ui.tiles->setTile(i, mTileCacheGetTile(cache, i, m_paletteId));
		}
	}
}
#endif

void TileView::updatePalette(int palette) {
	m_paletteId = palette;
	m_ui.tile->setPalette(palette);
	updateTiles(true);
}
