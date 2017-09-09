/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ObjView.h"

#include "GBAApp.h"

#include <QFontDatabase>
#include <QTimer>

#include "LogController.h"
#include "VFileDevice.h"

#ifdef M_CORE_GBA
#include <mgba/internal/gba/gba.h>
#endif
#ifdef M_CORE_GB
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/io.h>
#endif
#include <mgba-util/png-io.h>

using namespace QGBA;

ObjView::ObjView(GameController* controller, QWidget* parent)
	: AssetView(controller, parent)
	, m_controller(controller)
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

	connect(m_ui.tiles, &TilePainter::indexPressed, this, &ObjView::translateIndex);
	connect(m_ui.objId, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &ObjView::selectObj);
	connect(m_ui.magnification, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this]() {
		updateTiles(true);
	});
#ifdef USE_PNG
	connect(m_ui.exportButton, &QAbstractButton::clicked, this, &ObjView::exportObj);
#else
	m_ui.exportButton->setVisible(false);
#endif
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
	m_ui.objId->setMaximum(127);
	const GBA* gba = static_cast<const GBA*>(m_controller->thread()->core->board);
	const GBAObj* obj = &gba->video.oam.obj[m_objId];

	unsigned shape = GBAObjAttributesAGetShape(obj->a);
	unsigned size = GBAObjAttributesBGetSize(obj->b);
	unsigned width = GBAVideoObjSizes[shape * 4 + size][0];
	unsigned height = GBAVideoObjSizes[shape * 4 + size][1];
	unsigned tile = GBAObjAttributesCGetTile(obj->c);
	m_ui.tiles->setTileCount(width * height / 64);
	m_ui.tiles->setMinimumSize(QSize(width, height) * m_ui.magnification->value());
	m_ui.tiles->resize(QSize(width, height) * m_ui.magnification->value());
	unsigned palette = GBAObjAttributesCGetPalette(obj->c);
	unsigned tileBase = tile;
	unsigned paletteSet;
	unsigned bits;
	if (GBAObjAttributesAIs256Color(obj->a)) {
		m_ui.palette->setText("256-color");
		paletteSet = 1;
		m_ui.tile->setPalette(0);
		m_ui.tile->setPaletteSet(1, 1024, 1536);
		palette = 1;
		tile = tile / 2 + 1024;
		bits = 8;
	} else {
		m_ui.palette->setText(QString::number(palette));
		paletteSet = 0;
		m_ui.tile->setPalette(palette);
		m_ui.tile->setPaletteSet(0, 2048, 3072);
		palette += 16;
		tile += 2048;
		bits = 4;
	}
	ObjInfo newInfo{
		tile,
		width / 8,
		height / 8,
		width / 8,
		palette,
		paletteSet,
		bits
	};
	if (newInfo != m_objInfo) {
		force = true;
	}
	GBARegisterDISPCNT dispcnt = gba->memory.io[0]; // FIXME: Register name can't be imported due to namespacing issues
	if (!GBARegisterDISPCNTIsObjCharacterMapping(dispcnt)) {
		newInfo.stride = 0x20 >> (GBAObjAttributesAGet256Color(obj->a));
	};
	m_objInfo = newInfo;
	m_tileOffset = tile;
	mTileCacheSetPalette(m_tileCache.get(), paletteSet);

	int i = 0;
	for (int y = 0; y < height / 8; ++y) {
		for (int x = 0; x < width / 8; ++x, ++i, ++tile, ++tileBase) {
			const uint16_t* data = mTileCacheGetTileIfDirty(m_tileCache.get(), &m_tileStatus[32 * tileBase], tile, palette);
			if (data) {
				m_ui.tiles->setTile(i, data);
			} else if (force) {
				m_ui.tiles->setTile(i, mTileCacheGetTile(m_tileCache.get(), tile, palette));
			}
		}
		tile += newInfo.stride - width / 8;
		tileBase += newInfo.stride - width / 8;
	}

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
	m_ui.objId->setMaximum(39);
	const GB* gb = static_cast<const GB*>(m_controller->thread()->core->board);
	const GBObj* obj = &gb->video.oam.obj[m_objId];

	unsigned width = 8;
	unsigned height = 8;
	GBRegisterLCDC lcdc = gb->memory.io[REG_LCDC];
	if (GBRegisterLCDCIsObjSize(lcdc)) {
		height = 16;
	}
	unsigned tile = obj->tile;
	m_ui.tiles->setTileCount(width * height / 64);
	m_ui.tiles->setMinimumSize(QSize(width, height) * m_ui.magnification->value());
	m_ui.tiles->resize(QSize(width, height) * m_ui.magnification->value());
	unsigned palette = 0;
	if (gb->model >= GB_MODEL_CGB) {
		if (GBObjAttributesIsBank(obj->attr)) {
			tile += 512;
		}
		palette = GBObjAttributesGetCGBPalette(obj->attr);
	} else {
		palette = GBObjAttributesGetPalette(obj->attr);
	}
	m_ui.palette->setText(QString::number(palette));
	palette += 8;

	ObjInfo newInfo{
		tile,
		1,
		height / 8,
		1,
		palette,
		0,
		2
	};
	if (newInfo != m_objInfo) {
		force = true;
	}
	m_objInfo = newInfo;
	m_tileOffset = tile;

	int i = 0;
	mTileCacheSetPalette(m_tileCache.get(), 0);
	m_ui.tile->setPalette(palette);
	m_ui.tile->setPaletteSet(0, 512, 1024);
	for (int y = 0; y < height / 8; ++y, ++i) {
		unsigned t = tile + i;
		const uint16_t* data = mTileCacheGetTileIfDirty(m_tileCache.get(), &m_tileStatus[16 * t], t, palette);
		if (data) {
			m_ui.tiles->setTile(i, data);
		} else if (force) {
			m_ui.tiles->setTile(i, mTileCacheGetTile(m_tileCache.get(), t, palette));
		}
	}

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

#ifdef USE_PNG
void ObjView::exportObj() {
	QString filename = GBAApp::app()->getSaveFileName(this, tr("Export sprite"),
	                                                  tr("Portable Network Graphics (*.png)"));
	VFile* vf = VFileDevice::open(filename, O_WRONLY | O_CREAT | O_TRUNC);
	if (!vf) {
		LOG(QT, ERROR) << tr("Failed to open output PNG file: %1").arg(filename);
		return;
	}

	GameController::Interrupter interrupter(m_controller);
	mTileCacheSetPalette(m_tileCache.get(), m_objInfo.paletteSet);
	png_structp png = PNGWriteOpen(vf);
	png_infop info = PNGWriteHeader8(png, m_objInfo.width * 8, m_objInfo.height * 8);

	const uint16_t* rawPalette = mTileCacheGetPalette(m_tileCache.get(), m_objInfo.paletteId);
	unsigned colors = 1 << m_objInfo.bits;
	uint32_t palette[256];
	for (unsigned c = 0; c < colors && c < 256; ++c) {
		uint16_t color = rawPalette[c];
		palette[c] = M_R8(rawPalette[c]);
		palette[c] |= M_G8(rawPalette[c]) << 8;
		palette[c] |= M_B8(rawPalette[c]) << 16;
		if (c) {
			palette[c] |= 0xFF000000;
		}
	}
	PNGWritePalette(png, info, palette, colors);

	uint8_t* buffer = new uint8_t[m_objInfo.width * m_objInfo.height * 8 * 8];
	unsigned t = m_objInfo.tile;
	for (int y = 0; y < m_objInfo.height; ++y) {
		for (int x = 0; x < m_objInfo.width; ++x, ++t) {
			compositeTile(t, static_cast<void*>(buffer), m_objInfo.width * 8, x * 8, y * 8, m_objInfo.bits);
		}
		t += m_objInfo.stride - m_objInfo.width;
	}
	PNGWritePixels8(png, m_objInfo.width * 8, m_objInfo.height * 8, m_objInfo.width * 8, static_cast<void*>(buffer));
	PNGWriteClose(png, info);
	delete[] buffer;
}
#endif

bool ObjView::ObjInfo::operator!=(const ObjInfo& other) {
	return other.tile != tile ||
		other.width != width ||
		other.height != height ||
		other.stride != stride ||
		other.paletteId != paletteId ||
		other.paletteSet != paletteSet;
}
