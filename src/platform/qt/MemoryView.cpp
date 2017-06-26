/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MemoryView.h"

#include "GameController.h"

#include <mgba/core/core.h>

using namespace QGBA;

MemoryView::MemoryView(GameController* controller, QWidget* parent)
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
	        m_ui.hexfield, static_cast<void (MemoryModel::*)(uint32_t)>(&MemoryModel::jumpToAddress));
	connect(m_ui.hexfield, &MemoryModel::selectionChanged, this, &MemoryView::updateSelection);

	connect(controller, &GameController::gameStopped, this, &QWidget::close);

	connect(controller, &GameController::frameAvailable, this, &MemoryView::update);
	connect(controller, &GameController::gamePaused, this, &MemoryView::update);
	connect(controller, &GameController::stateLoaded, this, &MemoryView::update);
	connect(controller, &GameController::rewound, this, &MemoryView::update);

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

	m_ui.segments->setValue(-1);
	m_ui.segments->setVisible(info.maxSegment > 0);
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

void MemoryView::updateSelection(uint32_t start, uint32_t end) {
	m_selection.first = start;
	m_selection.second = end;
	updateStatus();
}

void MemoryView::updateStatus() {
	int align = m_ui.hexfield->alignment();
	if (!m_controller->isLoaded()) {
		return;
	}
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
