/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "AssetView.h"

#include "ui_ObjView.h"

#include <QList>

#include <mgba/core/tile-cache.h>

class QListWidgetItem;

namespace QGBA {

class CoreController;

class ObjView : public AssetView {
Q_OBJECT

public:
	ObjView(std::shared_ptr<CoreController> controller, QWidget* parent = nullptr);

public slots:
	void exportObj();
	void copyObj();

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

	void updateObjList(int maxObj);

	Ui::ObjView m_ui;

	std::shared_ptr<CoreController> m_controller;
	mTileCacheEntry m_tileStatus[1024 * 32] = {}; // TODO: Correct size
	int m_objId = 0;
	ObjInfo m_objInfo = {};

	QList<QListWidgetItem*> m_objs;

	int m_tileOffset;
	int m_boundary;
};

}
