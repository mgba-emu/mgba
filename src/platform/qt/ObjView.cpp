/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ObjView.h"

#include "CoreController.h"
#include "GBAApp.h"

#include <QAction>
#include <QClipboard>
#include <QListWidgetItem>
#include <QTimer>

#include "LogController.h"
#include "VFileDevice.h"

#ifdef M_CORE_GBA
#include <mgba/internal/gba/gba.h>
#endif
#ifdef M_CORE_GB
#include <mgba/internal/gb/gb.h>
#endif
#include <mgba-util/vfs.h>

using namespace QGBA;

ObjView::ObjView(std::shared_ptr<CoreController> controller, QWidget* parent)
	: AssetView(controller, parent)
	, m_controller(controller)
{
	m_ui.setupUi(this);
	m_ui.tile->setController(controller);

	const QFont font = GBAApp::app()->monospaceFont();

	m_ui.x->setFont(font);
	m_ui.y->setFont(font);
	m_ui.w->setFont(font);
	m_ui.h->setFont(font);
	m_ui.address->setFont(font);
	m_ui.priority->setFont(font);
	m_ui.palette->setFont(font);
	m_ui.transform->setFont(font);
	m_ui.xformPA->setFont(font);
	m_ui.xformPB->setFont(font);
	m_ui.xformPC->setFont(font);
	m_ui.xformPD->setFont(font);
	m_ui.mode->setFont(font);

	connect(m_ui.tiles, &TilePainter::indexPressed, this, &ObjView::translateIndex);
	connect(m_ui.tiles, &TilePainter::needsRedraw, this, [this]() {
		updateTiles(true);
	});
	connect(m_ui.objId, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &ObjView::selectObj);
	connect(m_ui.magnification, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this]() {
		updateTiles(true);
	});
	connect(m_ui.exportButton, &QAbstractButton::clicked, this, &ObjView::exportObj);
	connect(m_ui.copyButton, &QAbstractButton::clicked, this, &ObjView::copyObj);

	connect(m_ui.objList, &QListWidget::currentItemChanged, [this]() {
		QListWidgetItem* item = m_ui.objList->currentItem();
		if (item) {
			selectObj(item->data(Qt::UserRole).toInt());
		}
	});

	QAction* exportAction = new QAction(this);
	exportAction->setShortcut(QKeySequence::Save);
	connect(exportAction, &QAction::triggered, this, &ObjView::exportObj);
	addAction(exportAction);

	QAction* copyAction = new QAction(this);
	copyAction->setShortcut(QKeySequence::Copy);
	connect(copyAction, &QAction::triggered, this, &ObjView::copyObj);
	addAction(copyAction);
}

void ObjView::selectObj(int obj) {
	m_objId = obj;
	bool blocked = m_ui.objId->blockSignals(true);
	m_ui.objId->setValue(m_objId);
	m_ui.objId->blockSignals(blocked);
	if (m_objs.size() > obj) {
		blocked = m_ui.objList->blockSignals(true);
		m_ui.objList->setCurrentItem(m_objs[obj]);
		m_ui.objList->blockSignals(blocked);
	}
	updateTiles(true);
}

void ObjView::translateIndex(int index) {
	unsigned x = index % m_objInfo.width;
	unsigned y = index / m_objInfo.width;
	m_ui.tile->selectIndex(x + y * m_objInfo.stride + m_tileOffset + m_boundary);
}

#ifdef M_CORE_GBA
void ObjView::updateTilesGBA(bool force) {
	m_ui.objId->setMaximum(127);
	const GBA* gba = static_cast<const GBA*>(m_controller->thread()->core->board);
	const GBAObj* obj = &gba->video.oam.obj[m_objId];

	updateObjList(128);

	ObjInfo newInfo;
	lookupObj(m_objId, &newInfo);

	m_ui.tiles->setTileCount(newInfo.width * newInfo.height);
	m_ui.tiles->setMinimumSize(QSize(newInfo.width * 8, newInfo.height * 8) * m_ui.magnification->value());
	m_ui.tiles->resize(QSize(newInfo.width * 8, newInfo.height * 8) * m_ui.magnification->value());
	unsigned tileBase = newInfo.tile;
	unsigned tile = newInfo.tile;
	if (GBAObjAttributesAIs256Color(obj->a)) {
		m_ui.palette->setText("256-color");
		m_ui.tile->setBoundary(1024, 1, 3);
		m_boundary = 1024;
		tileBase *= 2;
		m_ui.tile->setMaxTile(1536);
		m_ui.tile->setPalette(0);
	} else {
		m_ui.palette->setText(QString::number(newInfo.paletteId));
		m_ui.tile->setBoundary(2048, 0, 2);
		m_boundary = 2048;
		m_ui.tile->setMaxTile(3072);
		m_ui.tile->setPalette(newInfo.paletteId);
	}
	if (newInfo != m_objInfo) {
		force = true;
	}
	m_objInfo = newInfo;
	m_tileOffset = newInfo.tile;
	mTileCache* tileCache = mTileCacheSetGetPointer(&m_cacheSet->tiles, newInfo.paletteSet);
	unsigned maxTiles = mTileCacheSystemInfoGetMaxTiles(tileCache->sysConfig);
	int i = 0;
	for (unsigned y = 0; y < newInfo.height; ++y) {
		for (unsigned x = 0; x < newInfo.width; ++x, ++i, ++tile, ++tileBase) {
			if (tile < maxTiles) {
				const color_t* data = mTileCacheGetTileIfDirty(tileCache, &m_tileStatus[16 * tileBase], tile, newInfo.paletteId);
				if (data) {
					m_ui.tiles->setTile(i, data);
				} else if (force) {
					m_ui.tiles->setTile(i, mTileCacheGetTile(tileCache, tile, newInfo.paletteId));
				}
			} else {
				m_ui.tiles->clearTile(i);
			}
		}
		tile += newInfo.stride - newInfo.width;
		tileBase += newInfo.stride - newInfo.width;
	}

	m_ui.x->setText(QString::number(newInfo.x));
	m_ui.y->setText(QString::number(newInfo.y));
	m_ui.w->setText(QString::number(newInfo.width * 8));
	m_ui.h->setText(QString::number(newInfo.height * 8));

	m_ui.address->setText(tr("0x%0").arg(GBA_BASE_OAM + m_objId * sizeof(*obj), 8, 16, QChar('0')));
	m_ui.priority->setText(QString::number(newInfo.priority));
	m_ui.flippedH->setChecked(newInfo.hflip);
	m_ui.flippedV->setChecked(newInfo.vflip);
	m_ui.enabled->setChecked(newInfo.enabled);
	m_ui.doubleSize->setChecked(GBAObjAttributesAIsDoubleSize(obj->a) && GBAObjAttributesAIsTransformed(obj->a));
	m_ui.mosaic->setChecked(GBAObjAttributesAIsMosaic(obj->a));

	if (GBAObjAttributesAIsTransformed(obj->a)) {
		int mtxId = GBAObjAttributesBGetMatIndex(obj->b);
		struct GBAOAMMatrix mat;
		LOAD_16LE(mat.a, 0, &gba->video.oam.mat[mtxId].a);
		LOAD_16LE(mat.b, 0, &gba->video.oam.mat[mtxId].b);
		LOAD_16LE(mat.c, 0, &gba->video.oam.mat[mtxId].c);
		LOAD_16LE(mat.d, 0, &gba->video.oam.mat[mtxId].d);
		m_ui.transform->setText(QString::number(mtxId));
		m_ui.xformPA->setText(QString("%0").arg(mat.a / 256., 5, 'f', 2));
		m_ui.xformPB->setText(QString("%0").arg(mat.b / 256., 5, 'f', 2));
		m_ui.xformPC->setText(QString("%0").arg(mat.c / 256., 5, 'f', 2));
		m_ui.xformPD->setText(QString("%0").arg(mat.d / 256., 5, 'f', 2));
	} else {
		m_ui.transform->setText(tr("Off"));
		m_ui.xformPA->setText(tr("---"));
		m_ui.xformPB->setText(tr("---"));
		m_ui.xformPC->setText(tr("---"));
		m_ui.xformPD->setText(tr("---"));
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

	updateObjList(40);

	ObjInfo newInfo;
	lookupObj(m_objId, &newInfo);

	mTileCache* tileCache = mTileCacheSetGetPointer(&m_cacheSet->tiles, 0);
	unsigned tile = newInfo.tile;
	m_ui.tiles->setTileCount(newInfo.height);
	m_ui.tile->setBoundary(1024, 0, 0);
	m_ui.tiles->setMinimumSize(QSize(8, newInfo.height * 8) * m_ui.magnification->value());
	m_ui.tiles->resize(QSize(8, newInfo.height * 8) * m_ui.magnification->value());
	m_ui.palette->setText(QString::number(newInfo.paletteId - 8));

	if (newInfo != m_objInfo) {
		force = true;
	}
	m_objInfo = newInfo;
	m_tileOffset = tile;
	m_boundary = 1024;
	m_ui.tile->setMaxTile(512);

	int i = 0;
	m_ui.tile->setPalette(newInfo.paletteId);
	for (unsigned y = 0; y < newInfo.height; ++y, ++i) {
		unsigned t = tile + i;
		const color_t* data = mTileCacheGetTileIfDirty(tileCache, &m_tileStatus[8 * t], t, newInfo.paletteId);
		if (data) {
			m_ui.tiles->setTile(i, data);
		} else if (force) {
			m_ui.tiles->setTile(i, mTileCacheGetTile(tileCache, t, newInfo.paletteId));
		}
	}

	m_ui.x->setText(QString::number(newInfo.x));
	m_ui.y->setText(QString::number(newInfo.y));
	m_ui.w->setText(QString::number(8));
	m_ui.h->setText(QString::number(newInfo.height * 8));

	m_ui.address->setText(tr("0x%0").arg(GB_BASE_OAM + m_objId * sizeof(*obj), 4, 16, QChar('0')));
	m_ui.priority->setText(QString::number(newInfo.priority));
	m_ui.flippedH->setChecked(newInfo.hflip);
	m_ui.flippedV->setChecked(newInfo.vflip);
	m_ui.enabled->setChecked(newInfo.enabled);
	m_ui.doubleSize->setChecked(false);
	m_ui.mosaic->setChecked(false);
	m_ui.transform->setText(tr("N/A"));
	m_ui.xformPA->setText(tr("---"));
	m_ui.xformPB->setText(tr("---"));
	m_ui.xformPC->setText(tr("---"));
	m_ui.xformPD->setText(tr("---"));
	m_ui.mode->setText(tr("N/A"));
}
#endif

void ObjView::updateObjList(int maxObj) {
	for (int i = 0; i < maxObj; ++i) {
		if (m_objs.size() <= i) {
			QListWidgetItem* item = new QListWidgetItem;
			item->setText(QString::number(i));
			item->setData(Qt::UserRole, i);
			item->setSizeHint(QSize(64, 96));
			item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
			if (m_objId == i) {
				item->setSelected(true);
			}
			m_objs.append(item);
			m_ui.objList->addItem(item);
		}
		QListWidgetItem* item = m_objs[i];
		ObjInfo info;
		lookupObj(i, &info);
		item->setIcon(QPixmap::fromImage(compositeObj(info)));
	}
}

void ObjView::exportObj() {
	QString filename = GBAApp::app()->getSaveFileName(this, tr("Export sprite"),
	                                                  tr("Portable Network Graphics (*.png)"));
	if (filename.isNull()) {
		return;
	}
	CoreController::Interrupter interrupter(m_controller);
	QImage obj = compositeObj(m_objInfo);
	obj.save(filename, "PNG");
}

void ObjView::copyObj() {
	CoreController::Interrupter interrupter(m_controller);
	GBAApp::app()->clipboard()->setImage(compositeObj(m_objInfo));
}
