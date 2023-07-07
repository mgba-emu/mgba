/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "TileView.h"

#include "CoreController.h"
#include "GBAApp.h"

#include <QAction>
#include <QClipboard>
#include <QTimer>

#ifdef M_CORE_GB
#include <mgba/internal/gb/gb.h>
#endif

using namespace QGBA;

TileView::TileView(std::shared_ptr<CoreController> controller, QWidget* parent)
	: AssetView(controller, parent)
	, m_controller(controller)
{
	m_ui.setupUi(this);
	m_ui.tile->setController(controller);

	connect(m_ui.tiles, &TilePainter::indexPressed, this, [this](int index) {
		if (m_ui.tilesObj->isChecked()) {
			switch (m_controller->platform()) {
#ifdef M_CORE_GBA
			case mPLATFORM_GBA:
				index += 2048 >> m_ui.palette256->isChecked();
				break;
#endif
			default:
				break;
			}
		}
		m_ui.tile->selectIndex(index);
	});
	connect(m_ui.tiles, &TilePainter::needsRedraw, this, [this]() {
		updateTiles(true);
	});
	connect(m_ui.tilesSelector, qOverload<QAbstractButton*>(&QButtonGroup::buttonClicked), this, [this]() {
		updateTiles(true);
	});
	connect(m_ui.paletteId, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &TileView::updatePalette);

	switch (m_controller->platform()) {
#ifdef M_CORE_GBA
	case mPLATFORM_GBA:
		m_ui.tile->setBoundary(2048, 0, 2);
		m_ui.tile->setMaxTile(3072);
		break;
#endif
#ifdef M_CORE_GB
	case mPLATFORM_GB:
		m_ui.tilesBg->setEnabled(false);
		m_ui.tilesObj->setEnabled(false);
		m_ui.tilesBoth->setEnabled(false);
		m_ui.palette256->setEnabled(false);
		m_ui.tile->setBoundary(1024, 0, 0);
		m_ui.tile->setMaxTile(512);
		break;
#endif
	default:
		return;
	}

	connect(m_ui.palette256, &QAbstractButton::toggled, [this](bool selected) {
		if (selected) {
			m_ui.paletteId->setValue(0);
		}
		switch (m_controller->platform()) {
#ifdef M_CORE_GBA
		case mPLATFORM_GBA:
			m_ui.tile->setBoundary(2048 >> selected, selected, selected + 2);
			m_ui.tile->setMaxTile(3072 >> selected);
			break;
#endif
#ifdef M_CORE_GB
		case mPLATFORM_GB:
			return;
#endif
		default:
			break;
		}
		updateTiles(true);
	});
	connect(m_ui.magnification, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int mag) {
		if (!m_ui.tileFit->isChecked()) {
			m_ui.tiles->setMinimumSize(mag * 8 * m_ui.tilesPerRow->value(), m_ui.tiles->minimumSize().height());
		}
		updateTiles(true);
	});

	connect(m_ui.tilesPerRow, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int count) {
		m_ui.tiles->setMinimumSize(m_ui.magnification->value() * 8 * count, m_ui.tiles->minimumSize().height());
		updateTiles(true);
	});

	connect(m_ui.tileFit, &QAbstractButton::toggled, [this](bool selected) {
		if (!selected) {
			m_ui.tiles->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
			m_ui.tiles->setMinimumSize(m_ui.magnification->value() * 8 * m_ui.tilesPerRow->value(), m_ui.tiles->minimumSize().height());
		} else {
			m_ui.tiles->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
		}
		updateTiles(true);
	});

	connect(m_ui.exportAll, &QAbstractButton::clicked, this, &TileView::exportTiles);
	connect(m_ui.exportOne, &QAbstractButton::clicked, this, &TileView::exportTile);
	connect(m_ui.copyAll, &QAbstractButton::clicked, this, &TileView::copyTiles);
	connect(m_ui.copyOne, &QAbstractButton::clicked, this, &TileView::copyTile);

	QAction* exportAll = new QAction(this);
	exportAll->setShortcut(QKeySequence::Save);
	connect(exportAll, &QAction::triggered, this, &TileView::exportTiles);
	addAction(exportAll);

	QAction* copyOne = new QAction(this);
	copyOne->setShortcut(QKeySequence::Copy);
	connect(copyOne, &QAction::triggered, this, &TileView::copyTile);
	addAction(copyOne);
}

#ifdef M_CORE_GBA
void TileView::updateTilesGBA(bool force) {
	if (m_ui.palette256->isChecked()) {
		if (m_ui.tilesBg->isChecked()) {
			m_ui.tiles->setTileCount(1024);
		} else if (m_ui.tilesObj->isChecked()) {
			m_ui.tiles->setTileCount(512);			
		} else {
			m_ui.tiles->setTileCount(1536);
		}
		mTileCache* cache;
		int objOffset = 1024;
		if (!m_ui.tilesObj->isChecked()) {
			objOffset = 0;
			cache = mTileCacheSetGetPointer(&m_cacheSet->tiles, 1);
			for (int i = 0; i < 1024; ++i) {
				const color_t* data = mTileCacheGetTileIfDirty(cache, &m_tileStatus[16 * i], i, 0);
				if (data) {
					m_ui.tiles->setTile(i, data);
				} else if (force) {
					m_ui.tiles->setTile(i, mTileCacheGetTile(cache, i, 0));
				}
			}
		}
		if (!m_ui.tilesBg->isChecked()) {
			cache = mTileCacheSetGetPointer(&m_cacheSet->tiles, 3);
			for (int i = 1024; i < 1536; ++i) {
				const color_t* data = mTileCacheGetTileIfDirty(cache, &m_tileStatus[16 * i], i - 1024, 0);
				if (data) {
					m_ui.tiles->setTile(i - objOffset, data);
				} else if (force) {
					m_ui.tiles->setTile(i - objOffset, mTileCacheGetTile(cache, i - 1024, 0));
				}
			}
		}
	} else {
		if (m_ui.tilesBg->isChecked()) {
			m_ui.tiles->setTileCount(2048);
		} else if (m_ui.tilesObj->isChecked()) {
			m_ui.tiles->setTileCount(1024);			
		} else {
			m_ui.tiles->setTileCount(3072);
		}
		mTileCache* cache;
		int objOffset = 2048;
		if (!m_ui.tilesObj->isChecked()) {
			objOffset = 0;
			cache = mTileCacheSetGetPointer(&m_cacheSet->tiles, 0);
			for (int i = 0; i < 2048; ++i) {
				const color_t* data = mTileCacheGetTileIfDirty(cache, &m_tileStatus[16 * i], i, m_paletteId);
				if (data) {
					m_ui.tiles->setTile(i, data);
				} else if (force) {
					m_ui.tiles->setTile(i, mTileCacheGetTile(cache, i, m_paletteId));
				}
			}
		}
		if (!m_ui.tilesBg->isChecked()) {
			cache = mTileCacheSetGetPointer(&m_cacheSet->tiles, 2);
			for (int i = 2048; i < 3072; ++i) {
				const color_t* data = mTileCacheGetTileIfDirty(cache, &m_tileStatus[16 * i], i - 2048, m_paletteId);
				if (data) {
					m_ui.tiles->setTile(i - objOffset, data);
				} else if (force) {
					m_ui.tiles->setTile(i - objOffset, mTileCacheGetTile(cache, i - 2048, m_paletteId));
				}
			}
		}
	}
}
#endif

#ifdef M_CORE_GB
void TileView::updateTilesGB(bool force) {
	const GB* gb = static_cast<const GB*>(m_controller->thread()->core->board);
	int count = gb->model >= GB_MODEL_CGB ? 1024 : 512;
	m_ui.tiles->setTileCount(count);
	mTileCache* cache = mTileCacheSetGetPointer(&m_cacheSet->tiles, 0);
	for (int i = 0; i < count; ++i) {
		const color_t* data = mTileCacheGetTileIfDirty(cache, &m_tileStatus[8 * i], i, m_paletteId);
		if (data) {
			m_ui.tiles->setTile(i, data);
		} else if (force) {
			m_ui.tiles->setTile(i, mTileCacheGetTile(cache, i, m_paletteId));
		}
	}
}
#endif

void TileView::updatePalette(int palette) {
	m_paletteId = palette;
	m_ui.tile->setPalette(palette);
	updateTiles(true);
}

void TileView::exportTiles() {
	QString filename = GBAApp::app()->getSaveFileName(this, tr("Export tiles"),
	                                                  tr("Portable Network Graphics (*.png)"));
	if (filename.isNull()) {
		return;
	}
	CoreController::Interrupter interrupter(m_controller);
	updateTiles(false);
	QPixmap pixmap(m_ui.tiles->backing());
	pixmap.save(filename, "PNG");
}

void TileView::exportTile() {
	QString filename = GBAApp::app()->getSaveFileName(this, tr("Export tile"),
	                                                  tr("Portable Network Graphics (*.png)"));
	if (filename.isNull()) {
		return;
	}
	CoreController::Interrupter interrupter(m_controller);
	updateTiles(false);
	QImage image(m_ui.tile->activeTile());
	image.save(filename, "PNG");
}

void TileView::copyTiles() {
	CoreController::Interrupter interrupter(m_controller);
	updateTiles(false);
	GBAApp::app()->clipboard()->setPixmap(m_ui.tiles->backing());
}

void TileView::copyTile() {
	CoreController::Interrupter interrupter(m_controller);
	updateTiles(false);
	GBAApp::app()->clipboard()->setImage(m_ui.tile->activeTile());
}
