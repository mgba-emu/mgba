/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ObjView.h"

#include "GBAApp.h"

#include <QFontDatabase>
#include <QTimer>

#include <mgba/internal/gba/gba.h>
#ifdef M_CORE_GB
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/io.h>
#endif

using namespace QGBA;

ObjView::ObjView(GameController* controller, QWidget* parent)
	: AssetView(controller, parent)
	, m_controller(controller)
	, m_tileStatus{}
	, m_objId(0)
	, m_objInfo{}
{
	m_ui.setupUi(this);
	m_ui.tile->setController(controller);

	const QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);

	m_ui.x->setFont(font);
	m_ui.y->setFont(font);
	m_ui.w->setFont(font);
	m_ui.h->setFont(font);
	m_ui.address->setFont(font);
	m_ui.priority->setFont(font);
	m_ui.palette->setFont(font);
	m_ui.transform->setFont(font);
	m_ui.mode->setFont(font);

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
	unsigned x = index % m_objInfo.width;
	unsigned y = index / m_objInfo.width;
	m_ui.tile->selectIndex(x + y * m_objInfo.stride + m_tileOffset);
}

#ifdef M_CORE_GBA
void ObjView::updateTilesGBA(bool force) {
	const GBA* gba = static_cast<const GBA*>(m_controller->thread()->core->board);
	const GBAObj* obj = &gba->video.oam.obj[m_objId];

	unsigned shape = GBAObjAttributesAGetShape(obj->a);
	unsigned size = GBAObjAttributesBGetSize(obj->b);
	unsigned width = GBAVideoObjSizes[shape * 4 + size][0];
	unsigned height = GBAVideoObjSizes[shape * 4 + size][1];
	unsigned tile = GBAObjAttributesCGetTile(obj->c);
	ObjInfo newInfo{
		tile,
		width / 8,
		height / 8,
		width / 8
	};
	m_ui.tiles->setTileCount(width * height / 64);
	m_ui.tiles->setMinimumSize(QSize(width, height) * m_ui.magnification->value());
	unsigned palette = GBAObjAttributesCGetPalette(obj->c);
	GBARegisterDISPCNT dispcnt = gba->memory.io[0]; // FIXME: Register name can't be imported due to namespacing issues
	if (!GBARegisterDISPCNTIsObjCharacterMapping(dispcnt)) {
		newInfo.stride = 0x20 >> (GBAObjAttributesAGet256Color(obj->a));
	};
	if (newInfo != m_objInfo) {
		force = true;
	}
	m_objInfo = newInfo;
	int i = 0;
	if (GBAObjAttributesAIs256Color(obj->a)) {
		m_ui.palette->setText("256-color");
		mTileCacheSetPalette(m_tileCache.get(), 1);
		m_ui.tile->setPalette(0);
		m_ui.tile->setPaletteSet(1, 1024, 1536);
		tile /= 2;
		unsigned t = tile + i;
		for (int y = 0; y < height / 8; ++y) {
			for (int x = 0; x < width / 8; ++x, ++i, ++t) {
				const uint16_t* data = mTileCacheGetTileIfDirty(m_tileCache.get(), &m_tileStatus[32 * t], t + 1024, 1);
				if (data) {
					m_ui.tiles->setTile(i, data);
				} else if (force) {
					m_ui.tiles->setTile(i, mTileCacheGetTile(m_tileCache.get(), t + 1024, 1));
				}
			}
			t += newInfo.stride - width / 8;
		}
		tile += 1024;
	} else {
		m_ui.palette->setText(QString::number(palette));
		mTileCacheSetPalette(m_tileCache.get(), 0);
		m_ui.tile->setPalette(palette);
		m_ui.tile->setPaletteSet(0, 2048, 3072);
		unsigned t = tile + i;
		for (int y = 0; y < height / 8; ++y) {
			for (int x = 0; x < width / 8; ++x, ++i, ++t) {
				const uint16_t* data = mTileCacheGetTileIfDirty(m_tileCache.get(), &m_tileStatus[32 * t], t + 2048, palette + 16);
				if (data) {
					m_ui.tiles->setTile(i, data);
				} else if (force) {
					m_ui.tiles->setTile(i, mTileCacheGetTile(m_tileCache.get(), t + 2048, palette + 16));
				}
			}
			t += newInfo.stride - width / 8;
		}
		tile += 2048;
	}
	m_tileOffset = tile;

	m_ui.x->setText(QString::number(GBAObjAttributesBGetX(obj->b)));
	m_ui.y->setText(QString::number(GBAObjAttributesAGetY(obj->a)));
	m_ui.w->setText(QString::number(width));
	m_ui.h->setText(QString::number(height));

	m_ui.address->setText(tr("0x%0").arg(BASE_OAM + m_objId * sizeof(*obj), 8, 16, QChar('0')));
	m_ui.priority->setText(QString::number(GBAObjAttributesCGetPriority(obj->c)));
	m_ui.flippedH->setChecked(GBAObjAttributesBIsHFlip(obj->b));
	m_ui.flippedV->setChecked(GBAObjAttributesBIsVFlip(obj->b));
	m_ui.enabled->setChecked(!GBAObjAttributesAIsDisable(obj->a) || GBAObjAttributesAIsTransformed(obj->a));
	m_ui.doubleSize->setChecked(GBAObjAttributesAIsDoubleSize(obj->a) && GBAObjAttributesAIsTransformed(obj->a));
	m_ui.mosaic->setChecked(GBAObjAttributesAIsMosaic(obj->a));

	if (GBAObjAttributesAIsTransformed(obj->a)) {
		m_ui.transform->setText(QString::number(GBAObjAttributesBGetMatIndex(obj->b)));
	} else {
		m_ui.transform->setText(tr("Off"));
	}

	switch (GBAObjAttributesAGetMode(obj->a)) {
	case OBJ_MODE_NORMAL:
		m_ui.mode->setText(tr("Normal"));
		break;
	case OBJ_MODE_SEMITRANSPARENT:
		m_ui.mode->setText(tr("Trans"));
		break;
	case OBJ_MODE_OBJWIN:
		m_ui.mode->setText(tr("OBJWIN"));
		break;
	default:
		m_ui.mode->setText(tr("Invalid"));
		break;
	}
}
#endif

#ifdef M_CORE_GB
void ObjView::updateTilesGB(bool force) {
	const GB* gb = static_cast<const GB*>(m_controller->thread()->core->board);
	const GBObj* obj = &gb->video.oam.obj[m_objId];

	unsigned width = 8;
	unsigned height = 8;
	GBRegisterLCDC lcdc = gb->memory.io[REG_LCDC];
	if (GBRegisterLCDCIsObjSize(lcdc)) {
		height = 16;
	}
	unsigned tile = obj->tile;
	ObjInfo newInfo{
		tile,
		1,
		height / 8,
		1
	};
	if (newInfo != m_objInfo) {
		force = true;
	}
	m_objInfo = newInfo;
	m_ui.tiles->setTileCount(width * height / 64);
	m_ui.tiles->setMinimumSize(QSize(width, height) * m_ui.magnification->value());
	int palette = 0;
	if (gb->model >= GB_MODEL_CGB) {
		if (GBObjAttributesIsBank(obj->attr)) {
			tile += 512;
		}
		palette = GBObjAttributesGetCGBPalette(obj->attr);
	} else {
		palette = GBObjAttributesGetPalette(obj->attr);
	}
	int i = 0;
	m_ui.palette->setText(QString::number(palette));
	mTileCacheSetPalette(m_tileCache.get(), 0);
	m_ui.tile->setPalette(palette + 8);
	m_ui.tile->setPaletteSet(0, 512, 1024);
	for (int y = 0; y < height / 8; ++y, ++i) {
		unsigned t = tile + i;
		const uint16_t* data = mTileCacheGetTileIfDirty(m_tileCache.get(), &m_tileStatus[16 * t], t, palette + 8);
		if (data) {
			m_ui.tiles->setTile(i, data);
		} else if (force) {
			m_ui.tiles->setTile(i, mTileCacheGetTile(m_tileCache.get(), t, palette + 8));
		}
	}
	m_tileOffset = tile;

	m_ui.x->setText(QString::number(obj->x));
	m_ui.y->setText(QString::number(obj->y));
	m_ui.w->setText(QString::number(width));
	m_ui.h->setText(QString::number(height));

	m_ui.address->setText(tr("0x%0").arg(GB_BASE_OAM + m_objId * sizeof(*obj), 4, 16, QChar('0')));
	m_ui.priority->setText(QString::number(GBObjAttributesGetPriority(obj->attr)));
	m_ui.flippedH->setChecked(GBObjAttributesIsXFlip(obj->attr));
	m_ui.flippedV->setChecked(GBObjAttributesIsYFlip(obj->attr));
	m_ui.enabled->setChecked(obj->y != 0 && obj->y < 160);
	m_ui.doubleSize->setChecked(false);
	m_ui.mosaic->setChecked(false);
	m_ui.transform->setText(tr("N/A"));
	m_ui.mode->setText(tr("N/A"));
}
#endif


bool ObjView::ObjInfo::operator!=(const ObjInfo& other) {
	return other.tile != tile ||
		other.width != width ||
		other.height != height ||
		other.stride != stride;
}
