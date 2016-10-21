/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ObjView.h"

#include "GBAApp.h"

#include <QFontDatabase>
#include <QTimer>

extern "C" {
#include "gba/gba.h"
}

using namespace QGBA;

ObjView::ObjView(GameController* controller, QWidget* parent)
	: AssetView(controller, parent)
	, m_controller(controller)
	, m_tileStatus{}
	, m_objId(0)
{
	m_ui.setupUi(this);
	m_ui.tile->setController(controller);

	const QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);

	m_ui.x->setFont(font);
	m_ui.y->setFont(font);
	m_ui.w->setFont(font);
	m_ui.h->setFont(font);
	m_ui.address->setFont(font);

	connect(m_ui.tiles, SIGNAL(indexPressed(int)), this, SLOT(translateIndex(int)));
	connect(m_ui.objId, SIGNAL(valueChanged(int)), this, SLOT(selectObj(int)));
	connect(m_ui.magnification, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this]() {
		updateTiles(true);
	});
}

void ObjView::selectObj(int obj) {
	m_objId = obj;
	updateTiles(true);
}

void ObjView::translateIndex(int index) {
	m_ui.tile->selectIndex(index + m_tileOffset);
}

#ifdef M_CORE_GBA
void ObjView::updateTilesGBA(bool force) {
	const GBA* gba = static_cast<const GBA*>(m_controller->thread()->core->board);
	const GBAObj* obj = &gba->video.oam.obj[m_objId];

	unsigned shape = GBAObjAttributesAGetShape(obj->a);
	unsigned size = GBAObjAttributesBGetSize(obj->b);
	unsigned width = GBAVideoObjSizes[shape * 4 + size][0];
	unsigned height = GBAVideoObjSizes[shape * 4 + size][1];
	m_ui.tiles->setTileCount(width * height / 64);
	m_ui.tiles->setMinimumSize(QSize(width, height) * m_ui.magnification->value());
	unsigned palette = GBAObjAttributesCGetPalette(obj->c);
	unsigned tile = GBAObjAttributesCGetTile(obj->c);
	int i = 0;
	// TODO: Tile stride
	// TODO: Check to see if parameters are changed (so as to enable force if needed)
	if (GBAObjAttributesAIs256Color(obj->a)) {
		mTileCacheSetPalette(m_tileCache.get(), 1);
		m_ui.tile->setPalette(0);
		m_ui.tile->setPaletteSet(1, 1024, 1536);
		tile /= 2;
		for (int y = 0; y < height / 8; ++y) {
			for (int x = 0; x < width / 8; ++x, ++i) {
				unsigned t = tile + i;
				const uint16_t* data = mTileCacheGetTileIfDirty(m_tileCache.get(), &m_tileStatus[32 * t], t + 1024, 1);
				if (data) {
					m_ui.tiles->setTile(i, data);
				} else if (force) {
					m_ui.tiles->setTile(i, mTileCacheGetTile(m_tileCache.get(), t + 1024, 1));
				}
			}
		}
		tile += 1024;
	} else {
		mTileCacheSetPalette(m_tileCache.get(), 0);
		m_ui.tile->setPalette(palette);
		m_ui.tile->setPaletteSet(0, 2048, 3072);
		for (int y = 0; y < height / 8; ++y) {
			for (int x = 0; x < width / 8; ++x, ++i) {
				unsigned t = tile + i;
				const uint16_t* data = mTileCacheGetTileIfDirty(m_tileCache.get(), &m_tileStatus[32 * t], t + 2048, palette + 16);
				if (data) {
					m_ui.tiles->setTile(i, data);
				} else if (force) {
					m_ui.tiles->setTile(i, mTileCacheGetTile(m_tileCache.get(), t + 2048, palette + 16));
				}
			}
		}
		tile += 2048;
	}
	m_tileOffset = tile;

	m_ui.x->setText(QString::number(GBAObjAttributesBGetX(obj->b)));
	m_ui.y->setText(QString::number(GBAObjAttributesAGetY(obj->a)));
	m_ui.w->setText(QString::number(width));
	m_ui.h->setText(QString::number(height));

	// TODO: Flags
}
#endif

#ifdef M_CORE_GB
void ObjView::updateTilesGB(bool force) {
}
#endif
