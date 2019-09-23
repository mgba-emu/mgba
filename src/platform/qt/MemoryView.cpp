/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MemoryView.h"

#include "CoreController.h"
#include "MemoryDump.h"

#include <mgba/core/core.h>

using namespace QGBA;

MemoryView::MemoryView(std::shared_ptr<CoreController> controller, QWidget* parent)
	: QWidget(parent)
	, m_controller(controller)
{
	m_ui.setupUi(this);

	m_ui.hexfield->setController(controller);

	mCore* core = m_controller->thread()->core;
	const mCoreMemoryBlock* info;
	size_t nBlocks = core->listMemoryBlocks(core, &info);

	connect(m_ui.regions, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
	        this, &MemoryView::setIndex);
	connect(m_ui.segments, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
	        this, &MemoryView::setSegment);

	if (info) {
		for (size_t i = 0; i < nBlocks; ++i) {
			if (!(info[i].flags & (mCORE_MEMORY_MAPPED | mCORE_MEMORY_VIRTUAL))) {
				continue;
			}
			m_ui.regions->addItem(tr(info[i].longName));
		}
	}

	connect(m_ui.width8, &QAbstractButton::clicked, [this]() { m_ui.hexfield->setAlignment(1); });
	connect(m_ui.width16, &QAbstractButton::clicked, [this]() { m_ui.hexfield->setAlignment(2); });
	connect(m_ui.width32, &QAbstractButton::clicked, [this]() { m_ui.hexfield->setAlignment(4); });
	connect(m_ui.setAddress, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
	        this, static_cast<void (MemoryView::*)(uint32_t)>(&MemoryView::jumpToAddress));
	connect(m_ui.hexfield, &MemoryModel::selectionChanged, this, &MemoryView::updateSelection);
	connect(m_ui.saveRange, &QAbstractButton::clicked, this, &MemoryView::saveRange);

	connect(controller.get(), &CoreController::stopping, this, &QWidget::close);

	connect(controller.get(), &CoreController::frameAvailable, this, &MemoryView::update);
	connect(controller.get(), &CoreController::paused, this, &MemoryView::update);
	connect(controller.get(), &CoreController::stateLoaded, this, &MemoryView::update);
	connect(controller.get(), &CoreController::rewound, this, &MemoryView::update);

	connect(m_ui.copy, &QAbstractButton::clicked, m_ui.hexfield, &MemoryModel::copy);
	connect(m_ui.save, &QAbstractButton::clicked, m_ui.hexfield, &MemoryModel::save);
	connect(m_ui.paste, &QAbstractButton::clicked, m_ui.hexfield, &MemoryModel::paste);
	connect(m_ui.load, &QAbstractButton::clicked, m_ui.hexfield, &MemoryModel::load);

	connect(m_ui.loadTBL, &QAbstractButton::clicked, m_ui.hexfield, &MemoryModel::loadTBL);
}

void MemoryView::setIndex(int index) {
	mCore* core = m_controller->thread()->core;
	const mCoreMemoryBlock* blocks;
	size_t nBlocks = core->listMemoryBlocks(core, &blocks);
	const mCoreMemoryBlock& info = blocks[index];

	m_region = qMakePair(info.start, info.end);
	m_ui.segments->setValue(-1);
	m_ui.segments->setVisible(info.maxSegment > 0);
	m_ui.segmentColon->setVisible(info.maxSegment > 0);
	m_ui.segments->setMaximum(info.maxSegment);
	m_ui.hexfield->setRegion(info.start, info.end - info.start, info.shortName);
}

void MemoryView::setSegment(int segment) {
	mCore* core = m_controller->thread()->core;
	const mCoreMemoryBlock* blocks;
	size_t nBlocks = core->listMemoryBlocks(core, &blocks);
	const mCoreMemoryBlock& info = blocks[m_ui.regions->currentIndex()];

	m_ui.hexfield->setSegment(info.maxSegment < segment ? info.maxSegment : segment);
}

void MemoryView::update() {
	m_ui.hexfield->viewport()->update();
	updateStatus();
}

void MemoryView::jumpToAddress(uint32_t address) {
	if (address < m_region.first || address >= m_region.second) {
		m_ui.regions->setCurrentIndex(0);
		setIndex(0);
	}
	if (address < m_region.first || address >= m_region.second) {
		return;
	}
	m_ui.hexfield->jumpToAddress(address);
}

void MemoryView::updateSelection(uint32_t start, uint32_t end) {
	m_selection.first = start;
	m_selection.second = end;
	updateStatus();
}

void MemoryView::updateStatus() {
	int align = m_ui.hexfield->alignment();
	mCore* core = m_controller->thread()->core;
	QByteArray selection(m_ui.hexfield->serialize());
	QString text(m_ui.hexfield->decodeText(selection));
	m_ui.stringVal->setText(text);

	if (m_selection.first & (align - 1) || m_selection.second - m_selection.first != align) {
		m_ui.sintVal->clear();
		m_ui.uintVal->clear();
		return;
	}
	union {
		uint32_t u32;
		int32_t i32;
		uint16_t u16;
		int16_t i16;
		uint8_t u8;
		int8_t i8;
	} value;
	switch (align) {
	case 1:
		value.u8 = core->rawRead8(core, m_selection.first, m_ui.segments->value());
		m_ui.sintVal->setText(QString::number(value.i8));
		m_ui.uintVal->setText(QString::number(value.u8));
		break;
	case 2:
		value.u16 = core->rawRead16(core, m_selection.first, m_ui.segments->value());
		m_ui.sintVal->setText(QString::number(value.i16));
		m_ui.uintVal->setText(QString::number(value.u16));
		break;
	case 4:
		value.u32 = core->rawRead32(core, m_selection.first, m_ui.segments->value());
		m_ui.sintVal->setText(QString::number(value.i32));
		m_ui.uintVal->setText(QString::number(value.u32));
		break;
	}
}

void MemoryView::saveRange() {
	MemoryDump* memdump = new MemoryDump(m_controller);
	memdump->setAttribute(Qt::WA_DeleteOnClose);
	memdump->setAddress(m_selection.first);
	memdump->setSegment(m_ui.segments->value());
	memdump->setByteCount(m_selection.second - m_selection.first);
	memdump->show();
}