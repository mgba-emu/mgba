/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "TileView.h"

#include "GBAApp.h"

#include <QFontDatabase>
#include <QTimer>

extern "C" {
#include "gba/gba.h"
}

using namespace QGBA;

TileView::TileView(GameController* controller, QWidget* parent)
	: QWidget(parent)
	, m_controller(controller)
	, m_tileStatus{}
	, m_tileCache(controller->tileCache())
	, m_paletteId(0)
{
	m_ui.setupUi(this);
	m_ui.tile->setController(controller);

	m_updateTimer.setSingleShot(true);
	m_updateTimer.setInterval(1);
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateTiles()));

	connect(m_controller, SIGNAL(frameAvailable(const uint32_t*)), &m_updateTimer, SLOT(start()));
	connect(m_controller, SIGNAL(gameStopped(mCoreThread*)), this, SLOT(close()));
	connect(m_ui.tiles, SIGNAL(indexPressed(int)), m_ui.tile, SLOT(selectIndex(int)));
	connect(m_ui.paletteId, SIGNAL(valueChanged(int)), this, SLOT(updatePalette(int)));

	int max = 1024;
	int boundary = 1024;
	switch (m_controller->platform()) {
#ifdef M_CORE_GBA
	case PLATFORM_GBA:
		max = 3072;
		boundary = 2048;
		break;
#endif
#ifdef M_CORE_GB
	case PLATFORM_GB:
		max = 1024;
		boundary = 1024;
		m_ui.palette256->setEnabled(false);
		break;
#endif
	default:
		return;
	}
	m_ui.tile->setPaletteSet(0, boundary, max);

	connect(m_ui.palette256, &QAbstractButton::toggled, [this](bool selected) {
		if (selected) {
			m_ui.paletteId->setValue(0);
		}
		int max = 1024;
		int boundary = 1024;
		switch (m_controller->platform()) {
#ifdef M_CORE_GBA
		case PLATFORM_GBA:
			max = 3072 >> selected;
			boundary = 2048 >> selected;
			break;
#endif
#ifdef M_CORE_GB
		case PLATFORM_GB:
			return;
#endif
		default:
			break;
		}
		m_ui.tile->setPaletteSet(selected, boundary, max);
		updateTiles(true);
	});
	connect(m_ui.magnification, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this]() {
		updateTiles(true);
	});
}

void TileView::updateTiles(bool force) {
	if (!m_controller->thread() || !m_controller->thread()->core) {
		return;
	}

	switch (m_controller->platform()) {
#ifdef M_CORE_GBA
	case PLATFORM_GBA:
		updateTilesGBA(force);
		break;
#endif
#ifdef M_CORE_GB
	case PLATFORM_GB:
		updateTilesGB(force);
		break;
#endif
	default:
		return;
	}
}

#ifdef M_CORE_GBA
void TileView::updateTilesGBA(bool force) {
	if (m_ui.palette256->isChecked()) {
		m_ui.tiles->setTileCount(1536);
		mTileCacheSetPalette(m_tileCache.get(), 1);
		for (int i = 0; i < 1024; ++i) {
			const uint16_t* data = mTileCacheGetTileIfDirty(m_tileCache.get(), &m_tileStatus[32 * i], i, 0);
			if (data) {
				m_ui.tiles->setTile(i, data);
			} else if (force) {
				m_ui.tiles->setTile(i, mTileCacheGetTile(m_tileCache.get(), i, 0));
			}
		}
		for (int i = 1024; i < 1536; ++i) {
			const uint16_t* data = mTileCacheGetTileIfDirty(m_tileCache.get(), &m_tileStatus[32 * i], i, 1);
			if (data) {
				m_ui.tiles->setTile(i, data);
			} else if (force) {
				m_ui.tiles->setTile(i, mTileCacheGetTile(m_tileCache.get(), i, 1));
			}
		}
	} else {
		m_ui.tiles->setTileCount(3072);
		mTileCacheSetPalette(m_tileCache.get(), 0);
		for (int i = 0; i < 2048; ++i) {
			const uint16_t* data = mTileCacheGetTileIfDirty(m_tileCache.get(), &m_tileStatus[32 * i], i, m_paletteId);
			if (data) {
				m_ui.tiles->setTile(i, data);
			} else if (force) {
				m_ui.tiles->setTile(i, mTileCacheGetTile(m_tileCache.get(), i, m_paletteId));
			}
		}
		for (int i = 2048; i < 3072; ++i) {
			const uint16_t* data = mTileCacheGetTileIfDirty(m_tileCache.get(), &m_tileStatus[32 * i], i, m_paletteId + 16);
			if (data) {
				m_ui.tiles->setTile(i, data);
			} else if (force) {
				m_ui.tiles->setTile(i, mTileCacheGetTile(m_tileCache.get(), i, m_paletteId + 16));
			}
		}
	}
}
#endif

#ifdef M_CORE_GB
void TileView::updateTilesGB(bool force) {
	m_ui.tiles->setTileCount(1024);
	mTileCacheSetPalette(m_tileCache.get(), 0);
	for (int i = 0; i < 1024; ++i) {
		const uint16_t* data = mTileCacheGetTileIfDirty(m_tileCache.get(), &m_tileStatus[16 * i], i, m_paletteId);
		if (data) {
			m_ui.tiles->setTile(i, data);
		} else if (force) {
			m_ui.tiles->setTile(i, mTileCacheGetTile(m_tileCache.get(), i, m_paletteId));
		}
	}
}
#endif

void TileView::updatePalette(int palette) {
	m_paletteId = palette;
	m_ui.tile->setPalette(palette);
	updateTiles(true);
}

void TileView::resizeEvent(QResizeEvent*) {
	updateTiles(true);
}

void TileView::showEvent(QShowEvent*) {
	updateTiles(true);
}
