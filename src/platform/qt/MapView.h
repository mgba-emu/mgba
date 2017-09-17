/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "AssetView.h"

#include "ui_MapView.h"

#include <mgba/core/map-cache.h>

namespace QGBA {

class CoreController;

class MapView : public AssetView {
Q_OBJECT

public:
	MapView(std::shared_ptr<CoreController> controller, QWidget* parent = nullptr);

private:
#ifdef M_CORE_GBA
	void updateTilesGBA(bool force) override;
#endif
#ifdef M_CORE_GB
	void updateTilesGB(bool force) override;
#endif

	Ui::MapView m_ui;

	std::shared_ptr<CoreController> m_controller;
	mMapCacheEntry m_mapStatus[1024 * 1024] = {}; // TODO: Correct size
};

}
