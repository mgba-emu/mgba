/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MemoryView.h"

#include "GameController.h"

#include <mgba/core/core.h>
#ifdef M_CORE_GBA
#include <mgba/internal/gba/memory.h>
#endif
#ifdef M_CORE_GB
#include <mgba/internal/gb/memory.h>
#endif

using namespace QGBA;

struct IndexInfo {
	const char* name;
	const char* longName;
	uint32_t base;
	uint32_t size;
	int maxSegment;
};
#ifdef M_CORE_GBA
const static struct IndexInfo indexInfoGBA[] = {
	{ "All", "All", 0, 0x10000000 },
	{ "BIOS", "BIOS (16kiB)", BASE_BIOS, SIZE_BIOS },
	{ "EWRAM", "Working RAM (256kiB)", BASE_WORKING_RAM, SIZE_WORKING_RAM },
	{ "IWRAM", "Internal Working RAM (32kiB)", BASE_WORKING_IRAM, SIZE_WORKING_IRAM },
	{ "MMIO", "Memory-Mapped I/O", BASE_IO, SIZE_IO },
	{ "Palette", "Palette RAM (1kiB)", BASE_PALETTE_RAM, SIZE_PALETTE_RAM },
	{ "VRAM", "Video RAM (96kiB)", BASE_VRAM, SIZE_VRAM },
	{ "OAM", "OBJ Attribute Memory (1kiB)", BASE_OAM, SIZE_OAM },
	{ "ROM", "Game Pak (32MiB)", BASE_CART0, SIZE_CART0 },
	{ "ROM WS1", "Game Pak (Waitstate 1)", BASE_CART1, SIZE_CART1 },
	{ "ROM WS2", "Game Pak (Waitstate 2)", BASE_CART2, SIZE_CART2 },
	{ "SRAM", "Static RAM (64kiB)", BASE_CART_SRAM, SIZE_CART_SRAM },
	{ nullptr, nullptr, 0, 0, 0 }
};
#endif
#ifdef M_CORE_GB
const static struct IndexInfo indexInfoGB[] = {
	{ "All", "All", 0, 0x10000 },
	{ "ROM", "Game Pak (32kiB)", GB_BASE_CART_BANK0, GB_SIZE_CART_BANK0 * 2, 511 },
	{ "VRAM", "Video RAM (8kiB)", GB_BASE_VRAM, GB_SIZE_VRAM, 1 },
	{ "SRAM", "External RAM (8kiB)", GB_BASE_EXTERNAL_RAM, GB_SIZE_EXTERNAL_RAM, 3 },
	{ "WRAM", "Working RAM (8kiB)", GB_BASE_WORKING_RAM_BANK0, GB_SIZE_WORKING_RAM_BANK0 * 2, 7 },
	{ "OAM", "OBJ Attribute Memory", GB_BASE_OAM, GB_SIZE_OAM },
	{ "IO", "Memory-Mapped I/O", GB_BASE_IO, GB_SIZE_IO },
	{ "HRAM", "High RAM", GB_BASE_HRAM, GB_SIZE_HRAM },
	{ nullptr, nullptr, 0, 0, 0 }
};
#endif

MemoryView::MemoryView(GameController* controller, QWidget* parent)
	: QWidget(parent)
	, m_controller(controller)
{
	m_ui.setupUi(this);

	m_ui.hexfield->setController(controller);

	mCore* core = m_controller->thread()->core;
	const IndexInfo* info = nullptr;
	switch (core->platform(core)) {
#ifdef M_CORE_GBA
	case PLATFORM_GBA:
		info = indexInfoGBA;
		break;
#endif
#ifdef M_CORE_GB
	case PLATFORM_GB:
		info = indexInfoGB;
		break;
#endif
	default:
		break;
	}

	connect(m_ui.regions, SIGNAL(currentIndexChanged(int)), this, SLOT(setIndex(int)));
	connect(m_ui.segments, SIGNAL(valueChanged(int)), this, SLOT(setSegment(int)));

	if (info) {
		for (size_t i = 0; info[i].name; ++i) {
			m_ui.regions->addItem(tr(info[i].longName));
		}
	}

	connect(m_ui.width8, &QAbstractButton::clicked, [this]() { m_ui.hexfield->setAlignment(1); });
	connect(m_ui.width16, &QAbstractButton::clicked, [this]() { m_ui.hexfield->setAlignment(2); });
	connect(m_ui.width32, &QAbstractButton::clicked, [this]() { m_ui.hexfield->setAlignment(4); });
	connect(m_ui.setAddress, SIGNAL(valueChanged(const QString&)), m_ui.hexfield, SLOT(jumpToAddress(const QString&)));
	connect(m_ui.hexfield, SIGNAL(selectionChanged(uint32_t, uint32_t)), this, SLOT(updateSelection(uint32_t, uint32_t)));

	connect(controller, SIGNAL(gameStopped(mCoreThread*)), this, SLOT(close()));

	connect(controller, SIGNAL(frameAvailable(const uint32_t*)), this, SLOT(update()));
	connect(controller, SIGNAL(gamePaused(mCoreThread*)), this, SLOT(update()));
	connect(controller, SIGNAL(stateLoaded(mCoreThread*)), this, SLOT(update()));
	connect(controller, SIGNAL(rewound(mCoreThread*)), this, SLOT(update()));

	connect(m_ui.copy, SIGNAL(clicked()), m_ui.hexfield, SLOT(copy()));
	connect(m_ui.save, SIGNAL(clicked()), m_ui.hexfield, SLOT(save()));
	connect(m_ui.paste, SIGNAL(clicked()), m_ui.hexfield, SLOT(paste()));
	connect(m_ui.load, SIGNAL(clicked()), m_ui.hexfield, SLOT(load()));

	connect(m_ui.loadTBL, SIGNAL(clicked()), m_ui.hexfield, SLOT(loadTBL()));
}

void MemoryView::setIndex(int index) {
	mCore* core = m_controller->thread()->core;
	IndexInfo info;
	switch (core->platform(core)) {
#ifdef M_CORE_GBA
	case PLATFORM_GBA:
		info = indexInfoGBA[index];
		break;
#endif
#ifdef M_CORE_GB
	case PLATFORM_GB:
		info = indexInfoGB[index];
		break;
#endif
	default:
		return;
	}
	m_ui.segments->setValue(-1);
	m_ui.segments->setVisible(info.maxSegment > 0);
	m_ui.segments->setMaximum(info.maxSegment);
	m_ui.hexfield->setRegion(info.base, info.size, info.name);
}

void MemoryView::setSegment(int segment) {
	mCore* core = m_controller->thread()->core;
	IndexInfo info;
	switch (core->platform(core)) {
#ifdef M_CORE_GBA
	case PLATFORM_GBA:
		info = indexInfoGBA[m_ui.regions->currentIndex()];
		break;
#endif
#ifdef M_CORE_GB
	case PLATFORM_GB:
		info = indexInfoGB[m_ui.regions->currentIndex()];
		break;
#endif
	default:
		return;
	}
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
