/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "PaletteView.h"

#include "GBAApp.h"
#include "LogController.h"
#include "VFileDevice.h"

#include <QFileDialog>
#include <QFontDatabase>

extern "C" {
#include "gba/supervisor/export.h"
#include "util/vfs.h"
}

using namespace QGBA;

PaletteView::PaletteView(GameController* controller, QWidget* parent)
	: QWidget(parent)
	, m_controller(controller)
{
	m_ui.setupUi(this);

	connect(m_controller, SIGNAL(frameAvailable(const uint32_t*)), this, SLOT(updatePalette()));
	m_ui.bgGrid->setDimensions(QSize(16, 16));
	m_ui.objGrid->setDimensions(QSize(16, 16));
	m_ui.selected->setSize(64);
	m_ui.selected->setDimensions(QSize(1, 1));
	updatePalette();

	const QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);

	m_ui.hexcode->setFont(font);
	m_ui.value->setFont(font);
	m_ui.index->setFont(font);
	m_ui.r->setFont(font);
	m_ui.g->setFont(font);
	m_ui.b->setFont(font);

	connect(m_ui.bgGrid, SIGNAL(indexPressed(int)), this, SLOT(selectIndex(int)));
	connect(m_ui.objGrid, &Swatch::indexPressed, [this] (int index) { selectIndex(index + 256); });
	connect(m_ui.exportBG, &QAbstractButton::clicked, [this] () { exportPalette(0, 256); });
	connect(m_ui.exportOBJ, &QAbstractButton::clicked, [this] () { exportPalette(256, 256); });

	connect(controller, SIGNAL(gameStopped(GBAThread*)), this, SLOT(close()));
}

void PaletteView::updatePalette() {
	if (!m_controller->thread() || !m_controller->thread()->gba) {
		return;
	}
	const uint16_t* palette = m_controller->thread()->gba->video.palette;
	for (int i = 0; i < 256; ++i) {
		m_ui.bgGrid->setColor(i, palette[i]);
		m_ui.objGrid->setColor(i, palette[i + 256]);
	}
	m_ui.bgGrid->update();
	m_ui.objGrid->update();
}

void PaletteView::selectIndex(int index) {
	uint16_t color = m_controller->thread()->gba->video.palette[index];
	m_ui.selected->setColor(0, color);
	uint32_t r = GBA_R5(color);
	uint32_t g = GBA_G5(color);
	uint32_t b = GBA_B5(color);
	uint32_t hexcode = (r << 19) | (g << 11) | (b << 3);
	m_ui.hexcode->setText(tr("#%0").arg(hexcode, 6, 16, QChar('0')));
	m_ui.value->setText(tr("0x%0").arg(color, 4, 16, QChar('0')));
	m_ui.index->setText(tr("%0").arg(index, 3, 10, QChar('0')));
	m_ui.r->setText(tr("0x%0 (%1)").arg(r, 2, 16, QChar('0')).arg(r, 2, 10, QChar('0')));
	m_ui.g->setText(tr("0x%0 (%1)").arg(g, 2, 16, QChar('0')).arg(g, 2, 10, QChar('0')));
	m_ui.b->setText(tr("0x%0 (%1)").arg(b, 2, 16, QChar('0')).arg(b, 2, 10, QChar('0')));
}

void PaletteView::exportPalette(int start, int length) {
	if (start >= 512) {
		return;
	}
	if (start + length > 512) {
		length = 512 - start;
	}
	m_controller->threadInterrupt();
	QFileDialog* dialog = GBAApp::app()->getSaveFileDialog(this, tr("Export palette"),
	                                                       tr("Windows PAL (*.pal);;Adobe Color Table (*.act)"));
	if (!dialog->exec()) {
		m_controller->threadContinue();
		return;
	}
	QString filename = dialog->selectedFiles()[0];
	VFile* vf = VFileDevice::open(filename, O_WRONLY | O_CREAT | O_TRUNC);
	if (!vf) {
		LOG(ERROR) << tr("Failed to open output palette file: %1").arg(filename);
		m_controller->threadContinue();
		return;
	}
	QString filter = dialog->selectedNameFilter();
	if (filter.contains("*.pal")) {
		GBAExportPaletteRIFF(vf, length, &m_controller->thread()->gba->video.palette[start]);
	} else if (filter.contains("*.act")) {
		GBAExportPaletteACT(vf, length, &m_controller->thread()->gba->video.palette[start]);
	}
	vf->close(vf);
	m_controller->threadContinue();
}
