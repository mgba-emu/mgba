/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_OBJ_VIEW
#define QGBA_OBJ_VIEW

#include "AssetView.h"
#include "GameController.h"

#include "ui_ObjView.h"

#include <mgba/core/tile-cache.h>

namespace QGBA {

class ObjView : public AssetView {
Q_OBJECT

public:
	ObjView(GameController* controller, QWidget* parent = nullptr);

#ifdef USE_PNG
public slots:
	void exportObj();
#endif

private slots:
	void selectObj(int);
	void translateIndex(int);

private:
#ifdef M_CORE_GBA
	void updateTilesGBA(bool force) override;
#endif
#ifdef M_CORE_GB
	void updateTilesGB(bool force) override;
#endif

	Ui::ObjView m_ui;

	GameController* m_controller;
	mTileCacheEntry m_tileStatus[1024 * 32] = {}; // TODO: Correct size
	int m_objId = 0;
	struct ObjInfo {
		unsigned tile;
		unsigned width;
		unsigned height;
		unsigned stride;
		unsigned paletteId;
		unsigned paletteSet;
		unsigned bits;

		bool operator!=(const ObjInfo&);
	} m_objInfo = {};

	int m_tileOffset;
};

}

#endif
