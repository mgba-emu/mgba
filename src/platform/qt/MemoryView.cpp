/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MemoryView.h"

#include "GameController.h"

extern "C" {
#include "gba/memory.h"
}

using namespace QGBA;

MemoryView::MemoryView(GameController* controller, QWidget* parent)
	: QWidget(parent)
	, m_controller(controller)
{
	m_ui.setupUi(this);

	m_ui.hexfield->setController(controller);

	connect(m_ui.regions, SIGNAL(currentIndexChanged(int)), this, SLOT(setIndex(int)));

	connect(m_ui.width8, &QAbstractButton::clicked, [this]() { m_ui.hexfield->setAlignment(1); });
	connect(m_ui.width16, &QAbstractButton::clicked, [this]() { m_ui.hexfield->setAlignment(2); });
	connect(m_ui.width32, &QAbstractButton::clicked, [this]() { m_ui.hexfield->setAlignment(4); });
	connect(m_ui.setAddress, SIGNAL(valueChanged(const QString&)), m_ui.hexfield, SLOT(jumpToAddress(const QString&)));

	connect(m_ui.hexfield, SIGNAL(selectionChanged(uint32_t, uint32_t)), this, SLOT(updateSelection(uint32_t, uint32_t)));

	connect(controller, SIGNAL(gameStopped(GBAThread*)), this, SLOT(close()));

	connect(controller, SIGNAL(frameAvailable(const uint32_t*)), this, SLOT(update()));
	connect(controller, SIGNAL(gamePaused(GBAThread*)), this, SLOT(update()));
	connect(controller, SIGNAL(stateLoaded(GBAThread*)), this, SLOT(update()));
	connect(controller, SIGNAL(rewound(GBAThread*)), this, SLOT(update()));
}

void MemoryView::setIndex(int index) {
	static struct {
		const char* name;
		uint32_t base;
		uint32_t size;
	} indexInfo[] = {
		{ "All", 0, 0x10000000 },
		{ "BIOS", BASE_BIOS, SIZE_BIOS },
		{ "EWRAM", BASE_WORKING_RAM, SIZE_WORKING_RAM },
		{ "IWRAM", BASE_WORKING_IRAM, SIZE_WORKING_IRAM },
		{ "MMIO", BASE_IO, SIZE_IO },
		{ "Palette", BASE_PALETTE_RAM, SIZE_PALETTE_RAM },
		{ "VRAM", BASE_VRAM, SIZE_VRAM },
		{ "OAM", BASE_OAM, SIZE_OAM },
		{ "ROM", BASE_CART0, SIZE_CART0 },
		{ "ROM WS1", BASE_CART1, SIZE_CART1 },
		{ "ROM WS2", BASE_CART2, SIZE_CART2 },
		{ "SRAM", BASE_CART_SRAM, SIZE_CART_SRAM },
	};
	const auto& info = indexInfo[index];
	m_ui.hexfield->setRegion(info.base, info.size, info.name);
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
	if (m_selection.first & (align - 1) || m_selection.second - m_selection.first != align) {
		m_ui.sintVal->clear();
		m_ui.uintVal->clear();
		return;
	}
	if (!m_controller->isLoaded()) {
		return;
	}
	ARMCore* cpu = m_controller->thread()->cpu;
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
		value.u8 = cpu->memory.load8(cpu, m_selection.first, nullptr);
		m_ui.sintVal->setText(QString::number(value.i8));
		m_ui.uintVal->setText(QString::number(value.u8));
		break;
	case 2:
		value.u16 = cpu->memory.load16(cpu, m_selection.first, nullptr);
		m_ui.sintVal->setText(QString::number(value.i16));
		m_ui.uintVal->setText(QString::number(value.u16));
		break;
	case 4:
		value.u32 = cpu->memory.load32(cpu, m_selection.first, nullptr);
		m_ui.sintVal->setText(QString::number(value.i32));
		m_ui.uintVal->setText(QString::number(value.u32));
		break;
	}
}
