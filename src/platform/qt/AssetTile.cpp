/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "AssetTile.h"

#include "GBAApp.h"

#include <QFontDatabase>

#include <mgba/core/interface.h>
#ifdef M_CORE_GBA
#include <mgba/internal/gba/memory.h>
#endif
#ifdef M_CORE_GB
#include <mgba/internal/gb/memory.h>
#endif

using namespace QGBA;

AssetTile::AssetTile(QWidget* parent)
	: QGroupBox(parent)
{
	m_ui.setupUi(this);

	m_ui.preview->setDimensions(QSize(8, 8));
	m_ui.color->setDimensions(QSize(1, 1));
	m_ui.color->setSize(50);

	connect(m_ui.preview, &Swatch::indexPressed, this, &AssetTile::selectColor);

	const QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);

	m_ui.tileId->setFont(font);
	m_ui.address->setFont(font);
	m_ui.r->setFont(font);
	m_ui.g->setFont(font);
	m_ui.b->setFont(font);
}

void AssetTile::setController(GameController* controller) {
	m_tileCache = controller->tileCache();
	switch (controller->platform()) {
#ifdef M_CORE_GBA
	case PLATFORM_GBA:
		m_addressWidth = 8;
		m_addressBase = BASE_VRAM;
		m_boundaryBase = BASE_VRAM | 0x10000;
		break;
#endif
#ifdef M_CORE_GB
	case PLATFORM_GB:
		m_addressWidth = 4;
		m_addressBase = GB_BASE_VRAM;
		m_boundaryBase = GB_BASE_VRAM;
		break;
#endif
	default:
		m_addressWidth = 0;
		m_addressBase = 0;
		m_boundaryBase = 0;
		break;
	}
}

void AssetTile::setPalette(int palette) {
	m_paletteId = palette;
	selectIndex(m_index);
}

void AssetTile::setPaletteSet(int palette, int boundary, int max) {
	m_index = m_index * (1 + m_paletteSet) / (1 + palette);
	if (m_index >= max) {
		m_index = max - 1;
	}
	m_boundary = boundary;
	m_paletteSet = palette;
	selectIndex(m_index);
}

void AssetTile::selectIndex(int index) {
	m_index = index;
	const uint16_t* data;

	mTileCacheSetPalette(m_tileCache.get(), m_paletteSet);
	unsigned bpp = 8 << m_tileCache->bpp;
	int dispIndex = index;
	int paletteId = m_paletteId;
	int base = m_addressBase;
	if (index >= m_boundary) {
		base = m_boundaryBase;
		// XXX: Do this better
#ifdef M_CORE_GBA
		if (m_boundaryBase == (BASE_VRAM | 0x10000)) {
			paletteId += m_tileCache->count / 2;
		}
#endif
		dispIndex -= m_boundary;
	}
	data = mTileCacheGetTile(m_tileCache.get(), index, paletteId);
	m_ui.tileId->setText(QString::number(dispIndex * (1 + m_paletteSet)));
	m_ui.address->setText(tr("%0%1%2")
		.arg(m_addressWidth == 4 ? index >= m_boundary : 0)
		.arg(m_addressWidth == 4 ? ":" : "x")
		.arg(dispIndex * bpp | base, m_addressWidth, 16, QChar('0')));
	for (int i = 0; i < 64; ++i) {
		m_ui.preview->setColor(i, data[i]);
	}
	m_ui.preview->update();
}

void AssetTile::selectColor(int index) {
	const uint16_t* data;
	mTileCacheSetPalette(m_tileCache.get(), m_paletteSet);
	unsigned bpp = 8 << m_tileCache->bpp;
	int paletteId = m_paletteId;
	// XXX: Do this better
#ifdef M_CORE_GBA
	if (m_index >= m_boundary && m_boundaryBase == (BASE_VRAM | 0x10000)) {
		paletteId += m_tileCache->count / 2;
	}
#endif
	data = mTileCacheGetTile(m_tileCache.get(), m_index, m_paletteId);
	uint16_t color = data[index];
	m_ui.color->setColor(0, color);
	m_ui.color->update();

	uint32_t r = M_R5(color);
	uint32_t g = M_G5(color);
	uint32_t b = M_B5(color);
	m_ui.r->setText(tr("0x%0 (%1)").arg(r, 2, 16, QChar('0')).arg(r, 2, 10, QChar('0')));
	m_ui.g->setText(tr("0x%0 (%1)").arg(g, 2, 16, QChar('0')).arg(g, 2, 10, QChar('0')));
	m_ui.b->setText(tr("0x%0 (%1)").arg(b, 2, 16, QChar('0')).arg(b, 2, 10, QChar('0')));
}

