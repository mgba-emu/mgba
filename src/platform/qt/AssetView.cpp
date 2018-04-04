/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "AssetView.h"

#include "CoreController.h"

#include <QTimer>

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

void AssetView::resizeEvent(QResizeEvent*) {
	updateTiles(true);
}

void AssetView::showEvent(QShowEvent*) {
	updateTiles(true);
}

void AssetView::compositeTile(const void* tBuffer, void* buffer, size_t stride, size_t x, size_t y, int depth) {
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
