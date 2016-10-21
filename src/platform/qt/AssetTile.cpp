/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "AssetTile.h"

#include "GBAApp.h"

#include <QFontDatabase>

extern "C" {
#ifdef M_CORE_GBA
#include "gba/memory.h"
#endif
#ifdef M_CORE_GB
#include "gb/memory.h"
#endif
}

using namespace QGBA;

AssetTile::AssetTile(QWidget* parent)
	: QGroupBox(parent)
	, m_tileCache(nullptr)
	, m_paletteId(0)
	, m_paletteSet(0)
	, m_index(0)
{
	m_ui.setupUi(this);

	m_ui.preview->setDimensions(QSize(8, 8));

	const QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);

	m_ui.tileId->setFont(font);
	m_ui.address->setFont(font);
}

void AssetTile::setController(GameController* controller) {
	m_tileCache = controller->tileCache();
	switch (controller->platform()) {
#ifdef M_CORE_GBA
	case PLATFORM_GBA:
		m_addressWidth = 8;
		m_addressBase = BASE_VRAM;
		break;
#endif
#ifdef M_CORE_GB
	case PLATFORM_GB:
		m_addressWidth = 4;
		m_addressBase = GB_BASE_VRAM;
		break;
#endif
	default:
		m_addressWidth = 0;
		m_addressBase = 0;
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
	m_ui.tileId->setText(QString::number(index * (1 + m_paletteSet)));

	mTileCacheSetPalette(m_tileCache.get(), m_paletteSet);
	unsigned bpp = 8 << m_tileCache->bpp;
	m_ui.address->setText(tr("0x%0").arg(index * bpp | m_addressBase, m_addressWidth, 16, QChar('0')));
	if (index < m_boundary) {
		data = mTileCacheGetTile(m_tileCache.get(), index, m_paletteId);
	} else {
		data = mTileCacheGetTile(m_tileCache.get(), index, m_paletteId + m_tileCache->count / 2);
	}
	for (int i = 0; i < 64; ++i) {
		m_ui.preview->setColor(i, data[i]);
	}
	m_ui.preview->update();
}

