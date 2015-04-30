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

	connect(controller, SIGNAL(gameStopped(GBAThread*)), this, SLOT(close()));
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
		{ "ROM (WS1)", BASE_CART1, SIZE_CART1 },
		{ "ROM (WS2)", BASE_CART2, SIZE_CART2 },
		{ "SRAM", BASE_CART_SRAM, SIZE_CART_SRAM },
	};
	const auto& info = indexInfo[index];
	m_ui.hexfield->setRegion(info.base, info.size, info.name);
}
