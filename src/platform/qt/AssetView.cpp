/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "AssetView.h"

#include <QTimer>

using namespace QGBA;

AssetView::AssetView(GameController* controller, QWidget* parent)
	: QWidget(parent)
	, m_controller(controller)
	, m_tileCache(controller->tileCache())
{
	m_updateTimer.setSingleShot(true);
	m_updateTimer.setInterval(1);
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateTiles()));

	connect(m_controller, SIGNAL(frameAvailable(const uint32_t*)), &m_updateTimer, SLOT(start()));
	connect(m_controller, SIGNAL(gameStopped(mCoreThread*)), this, SLOT(close()));
	connect(m_controller, SIGNAL(gameStopped(mCoreThread*)), &m_updateTimer, SLOT(stop()));
}

void AssetView::updateTiles(bool force) {
	if (!m_controller->isLoaded()) {
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

void AssetView::resizeEvent(QResizeEvent*) {
	updateTiles(true);
}

void AssetView::showEvent(QShowEvent*) {
	updateTiles(true);
}
