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
	, m_paletteId(0)
{
	m_ui.setupUi(this);
	GBAVideoTileCacheInit(&m_tileCache);

	m_ui.preview->setDimensions(QSize(8, 8));
	updateTiles();

	const QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);

	m_ui.tileId->setFont(font);
	m_ui.address->setFont(font);

	connect(m_controller, SIGNAL(frameAvailable(const uint32_t*)), this, SLOT(updateTiles()));
	connect(m_controller, SIGNAL(gameStopped(mCoreThread*)), this, SLOT(close()));
	connect(m_ui.tiles, SIGNAL(indexPressed(int)), this, SLOT(selectIndex(int)));
	connect(m_ui.paletteId, SIGNAL(valueChanged(int)), this, SLOT(updatePalette(int)));
	connect(m_ui.palette256, SIGNAL(toggled(bool)), this, SLOT(updateTiles()));
}

TileView::~TileView() {
	GBAVideoTileCacheDeinit(&m_tileCache);
}

void TileView::selectIndex(int index) {
	const uint16_t* data;
	m_ui.tileId->setText(QString::number(index));
	if (m_ui.palette256->isChecked()) {
		m_ui.address->setText(tr("0x%0").arg(index * 64 | BASE_VRAM, 8, 16, QChar('0')));
		if (index < 1024) {
			data = GBAVideoTileCacheGetTile256(&m_tileCache, index, 0);
		} else {
			data = GBAVideoTileCacheGetTile256(&m_tileCache, index, 1);
		}
	} else {
		m_ui.address->setText(tr("0x%0").arg(index * 32 | BASE_VRAM, 8, 16, QChar('0')));
		if (index < 2048) {
			data = GBAVideoTileCacheGetTile16(&m_tileCache, index, m_paletteId);
		} else {
			data = GBAVideoTileCacheGetTile16(&m_tileCache, index, m_paletteId + 16);
		}
	}
	for (int i = 0; i < 64; ++i) {
		m_ui.preview->setColor(i, data[i]);
	}
	m_ui.preview->update();
}

void TileView::updateTiles(bool force) {
	if (!m_controller->thread() || !m_controller->thread()->core) {
		return;
	}

	GBA* gba = static_cast<GBA*>(m_controller->thread()->core->board);
	GBAVideoTileCacheAssociate(&m_tileCache, &gba->video);

	if (m_ui.palette256->isChecked()) {
		m_ui.tiles->setTileCount(1536);
		for (int i = 0; i < 1024; ++i) {
			const uint16_t* data = GBAVideoTileCacheGetTile256IfDirty(&m_tileCache, i, 0);
			if (data) {
				m_ui.tiles->setTile(i, data);
			} else if (force) {
				m_ui.tiles->setTile(i, GBAVideoTileCacheGetTile256(&m_tileCache, i, 0));
			}
		}
		for (int i = 1024; i < 1536; ++i) {
			const uint16_t* data = GBAVideoTileCacheGetTile256IfDirty(&m_tileCache, i, 1);
			if (data) {
				m_ui.tiles->setTile(i, data);
			} else if (force) {
				m_ui.tiles->setTile(i, GBAVideoTileCacheGetTile256(&m_tileCache, i, 1));
			}
		}
	} else {
		m_ui.tiles->setTileCount(3072);
		for (int i = 0; i < 2048; ++i) {
			const uint16_t* data = GBAVideoTileCacheGetTile16IfDirty(&m_tileCache, i, m_paletteId);
			if (data) {
				m_ui.tiles->setTile(i, data);
			} else if (force) {
				m_ui.tiles->setTile(i, GBAVideoTileCacheGetTile16(&m_tileCache, i, m_paletteId));
			}
		}
		for (int i = 2048; i < 3072; ++i) {
			const uint16_t* data = GBAVideoTileCacheGetTile16IfDirty(&m_tileCache, i, m_paletteId + 16);
			if (data) {
				m_ui.tiles->setTile(i, data);
			} else if (force) {
				m_ui.tiles->setTile(i, GBAVideoTileCacheGetTile16(&m_tileCache, i, m_paletteId + 16));
			}
		}
	}
}

void TileView::updatePalette(int palette) {
	m_paletteId = palette;
	updateTiles();
}

void TileView::resizeEvent(QResizeEvent*) {
	updateTiles(true);
}

void TileView::showEvent(QShowEvent*) {
	// XXX: Figure out how to prevent the first resizeEvent
	QTimer::singleShot(10, [this]() {
		updateTiles(true);
	});
}
