/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "AssetView.h"

#include "CoreController.h"

#include <QTimer>

#ifdef M_CORE_GBA
#include <mgba/internal/gba/gba.h>
#endif
#ifdef M_CORE_GB
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/io.h>
#endif

#include <mgba/core/map-cache.h>

using namespace QGBA;

AssetView::AssetView(std::shared_ptr<CoreController> controller, QWidget* parent)
	: QWidget(parent)
	, m_cacheSet(controller->graphicCaches())
	, m_controller(controller)
{
	m_updateTimer.setSingleShot(true);
	m_updateTimer.setInterval(1);
	connect(&m_updateTimer, &QTimer::timeout, this, static_cast<void(AssetView::*)()>(&AssetView::updateTiles));

	connect(controller.get(), &CoreController::frameAvailable, &m_updateTimer,
	        static_cast<void(QTimer::*)()>(&QTimer::start));
	connect(controller.get(), &CoreController::stopping, this, &AssetView::close);
	connect(controller.get(), &CoreController::stopping, &m_updateTimer, &QTimer::stop);
}

void AssetView::updateTiles() {
	updateTiles(false);
}

void AssetView::updateTiles(bool force) {
	switch (m_controller->platform()) {
#ifdef M_CORE_GBA
	case mPLATFORM_GBA:
		updateTilesGBA(force);
		break;
#endif
#ifdef M_CORE_GB
	case mPLATFORM_GB:
		updateTilesGB(force);
		break;
#endif
	default:
		return;
	}
}

void AssetView::resizeEvent(QResizeEvent*) {
	updateTiles(true);
}

void AssetView::showEvent(QShowEvent*) {
	updateTiles(true);
}

void AssetView::compositeTile(const void* tBuffer, void* buffer, size_t stride, size_t x, size_t y, int depth) {
	if (!tBuffer) {
		return;
	}
	const uint8_t* tile = static_cast<const uint8_t*>(tBuffer);
	uint8_t* pixels = static_cast<uint8_t*>(buffer);
	size_t base = stride * y + x;
	switch (depth) {
	case 2:
		for (size_t i = 0; i < 8; ++i) {
			uint8_t tileDataLower = tile[i * 2];
			uint8_t tileDataUpper = tile[i * 2 + 1];
			uint8_t pixel;
			pixel = ((tileDataUpper & 128) >> 6) | ((tileDataLower & 128) >> 7);
			pixels[base + i * stride] = pixel;
			pixel = ((tileDataUpper & 64) >> 5) | ((tileDataLower & 64) >> 6);
			pixels[base + i * stride + 1] = pixel;
			pixel = ((tileDataUpper & 32) >> 4) | ((tileDataLower & 32) >> 5);
			pixels[base + i * stride + 2] = pixel;
			pixel = ((tileDataUpper & 16) >> 3) | ((tileDataLower & 16) >> 4);
			pixels[base + i * stride + 3] = pixel;
			pixel = ((tileDataUpper & 8) >> 2) | ((tileDataLower & 8) >> 3);
			pixels[base + i * stride + 4] = pixel;
			pixel = ((tileDataUpper & 4) >> 1) | ((tileDataLower & 4) >> 2);
			pixels[base + i * stride + 5] = pixel;
			pixel = (tileDataUpper & 2) | ((tileDataLower & 2) >> 1);
			pixels[base + i * stride + 6] = pixel;
			pixel = ((tileDataUpper & 1) << 1) | (tileDataLower & 1);
			pixels[base + i * stride + 7] = pixel;
		}
		break;
	case 4:
		for (size_t j = 0; j < 8; ++j) {
			for (size_t i = 0; i < 4; ++i) {
				pixels[base + j * stride + i * 2] =  tile[j * 4 + i] & 0xF;
				pixels[base + j * stride + i * 2 + 1] =  tile[j * 4 + i] >> 4;
			}
		}
		break;
	case 8:
		for (size_t i = 0; i < 8; ++i) {
			memcpy(&pixels[base + i * stride], &tile[i * 8], 8);
		}
		break;
	}
}

QImage AssetView::compositeMap(int map, QVector<mMapCacheEntry>* mapStatus) {
	mMapCache* mapCache = mMapCacheSetGetPointer(&m_cacheSet->maps, map);
	int tilesW = 1 << mMapCacheSystemInfoGetTilesWide(mapCache->sysConfig);
	int tilesH = 1 << mMapCacheSystemInfoGetTilesHigh(mapCache->sysConfig);
	if (mapStatus->size() != tilesW * tilesH) {
		mapStatus->resize(tilesW * tilesH);
		mapStatus->fill({});
	}
	QImage rawMap = QImage(QSize(tilesW * 8, tilesH * 8), QImage::Format_ARGB32);
	uchar* bgBits = rawMap.bits();
	for (int j = 0; j < tilesH; ++j) {
		for (int i = 0; i < tilesW; ++i) {
			mMapCacheCleanTile(mapCache, mapStatus->data(), i, j);
		}
		for (int i = 0; i < 8; ++i) {
			memcpy(static_cast<void*>(&bgBits[tilesW * 32 * (i + j * 8)]), mMapCacheGetRow(mapCache, i + j * 8), tilesW * 32);
		}
	}
	return rawMap.rgbSwapped();
}

QImage AssetView::compositeObj(const ObjInfo& objInfo) {
	mTileCache* tileCache = mTileCacheSetGetPointer(&m_cacheSet->tiles, objInfo.paletteSet);
	unsigned maxTiles = mTileCacheSystemInfoGetMaxTiles(tileCache->sysConfig);
	const color_t* rawPalette = mTileCacheGetPalette(tileCache, objInfo.paletteId);
	unsigned colors = 1 << objInfo.bits;
	QVector<QRgb> palette;

	palette.append(rawPalette[0] & 0xFFFFFF);
	for (unsigned c = 1; c < colors && c < 256; ++c) {
		palette.append(rawPalette[c] | 0xFF000000);
	}

	QImage image = QImage(QSize(objInfo.width * 8, objInfo.height * 8), QImage::Format_Indexed8);
	image.setColorTable(palette);
	image.fill(0);
	uchar* bits = image.bits();
	unsigned t = objInfo.tile;
	for (unsigned y = 0; y < objInfo.height && t < maxTiles; ++y) {
		for (unsigned x = 0; x < objInfo.width && t < maxTiles; ++x, ++t) {
			compositeTile(static_cast<const void*>(mTileCacheGetVRAM(tileCache, t)), bits, objInfo.width * 8, x * 8, y * 8, objInfo.bits);
		}
		t += objInfo.stride - objInfo.width;
	}
	return image.rgbSwapped();
}

bool AssetView::lookupObj(int id, struct ObjInfo* info) {
	switch (m_controller->platform()) {
#ifdef M_CORE_GBA
	case mPLATFORM_GBA:
		return lookupObjGBA(id, info);
#endif
#ifdef M_CORE_GB
	case mPLATFORM_GB:
		return lookupObjGB(id, info);
#endif
	default:
		return false;
	}
}

#ifdef M_CORE_GBA
bool AssetView::lookupObjGBA(int id, struct ObjInfo* info) {
	if (id > 127) {
		return false;
	}

	const GBA* gba = static_cast<const GBA*>(m_controller->thread()->core->board);
	const GBAObj* obj = &gba->video.oam.obj[id];

	unsigned shape = GBAObjAttributesAGetShape(obj->a);
	unsigned size = GBAObjAttributesBGetSize(obj->b);
	unsigned width = GBAVideoObjSizes[shape * 4 + size][0];
	unsigned height = GBAVideoObjSizes[shape * 4 + size][1];
	unsigned tile = GBAObjAttributesCGetTile(obj->c);
	unsigned palette = GBAObjAttributesCGetPalette(obj->c);
	unsigned paletteSet;
	unsigned bits;
	if (GBAObjAttributesAIs256Color(obj->a)) {
		paletteSet = 3;
		palette = 0;
		tile /= 2;
		bits = 8;
	} else {
		paletteSet = 2;
		bits = 4;
	}
	ObjInfo newInfo{
		tile,
		width / 8,
		height / 8,
		width / 8,
		palette,
		paletteSet,
		bits,
		!GBAObjAttributesAIsDisable(obj->a) || GBAObjAttributesAIsTransformed(obj->a),
		GBAObjAttributesCGetPriority(obj->c),
		GBAObjAttributesBGetX(obj->b),
		GBAObjAttributesAGetY(obj->a),
		false,
		false,
	};
	if (GBAObjAttributesAIsTransformed(obj->a)) {
		int matIndex = GBAObjAttributesBGetMatIndex(obj->b);
		const GBAOAMMatrix* mat = &gba->video.oam.mat[matIndex];
		QTransform invXform(mat->a / 256., mat->c / 256., mat->b / 256., mat->d / 256., 0, 0);
		newInfo.xform = invXform.inverted();
	} else {
		newInfo.hflip = bool(GBAObjAttributesBIsHFlip(obj->b));
		newInfo.vflip = bool(GBAObjAttributesBIsVFlip(obj->b));
	}
	GBARegisterDISPCNT dispcnt = gba->memory.io[0]; // FIXME: Register name can't be imported due to namespacing issues
	if (!GBARegisterDISPCNTIsObjCharacterMapping(dispcnt)) {
		newInfo.stride = 0x20 >> (GBAObjAttributesAGet256Color(obj->a));
	};
	*info = newInfo;
	return true;
}
#endif

#ifdef M_CORE_GB
bool AssetView::lookupObjGB(int id, struct ObjInfo* info) {
	if (id > 39) {
		return false;
	}

	const GB* gb = static_cast<const GB*>(m_controller->thread()->core->board);
	const GBObj* obj = &gb->video.oam.obj[id];

	unsigned height = 8;
	GBRegisterLCDC lcdc = gb->memory.io[GB_REG_LCDC];
	if (GBRegisterLCDCIsObjSize(lcdc)) {
		height = 16;
	}
	unsigned tile = obj->tile;
	unsigned palette = 0;
	if (gb->model >= GB_MODEL_CGB) {
		if (GBObjAttributesIsBank(obj->attr)) {
			tile += 512;
		}
		palette = GBObjAttributesGetCGBPalette(obj->attr);
	} else {
		palette = GBObjAttributesGetPalette(obj->attr);
	}
	palette += 8;

	ObjInfo newInfo{
		tile,
		1,
		height / 8,
		1,
		palette,
		0,
		2,
		obj->y != 0 && obj->y < 160 && obj->x != 0 && obj->x < 168,
		GBObjAttributesGetPriority(obj->attr),
		obj->x - 8,
		obj->y - 16,
		bool(GBObjAttributesIsXFlip(obj->attr)),
		bool(GBObjAttributesIsYFlip(obj->attr)),
	};
	*info = newInfo;
	return true;
}
#endif

bool AssetView::ObjInfo::operator!=(const ObjInfo& other) const {
	return other.tile != tile ||
		other.width != width ||
		other.height != height ||
		other.stride != stride ||
		other.paletteId != paletteId ||
		other.paletteSet != paletteSet;
}
