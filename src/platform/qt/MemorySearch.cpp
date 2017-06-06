/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MemorySearch.h"

#include <mgba/core/core.h>
#include <mgba/core/mem-search.h>

#include "GameController.h"

using namespace QGBA;

MemorySearch::MemorySearch(GameController* controller, QWidget* parent)
	: QWidget(parent)
	, m_controller(controller)
{
	m_ui.setupUi(this);

	connect(m_ui.search, &QPushButton::clicked, this, &MemorySearch::search);
}

void MemorySearch::search() {
	mCoreMemorySearchResults res;
	mCoreMemorySearchResultsInit(&res, 0);

	mCoreMemorySearchParams params;
	params.memoryFlags = mCORE_MEMORY_RW;

	GameController::Interrupter interrupter(m_controller);
	if (!m_controller->isLoaded()) {
		return;
	}
	mCore* core = m_controller->thread()->core;

	QByteArray string;
	if (m_ui.typeNum->isChecked()) {
		if (m_ui.bits8->isChecked()) {
			params.type = mCORE_MEMORY_SEARCH_8;
		}
		if (m_ui.bits16->isChecked()) {
			params.type = mCORE_MEMORY_SEARCH_16;
		}
		if (m_ui.bits32->isChecked()) {
			params.type = mCORE_MEMORY_SEARCH_32;
		}
		if (m_ui.numHex->isChecked()) {
			bool ok;
			uint32_t v = m_ui.value->text().toUInt(&ok, 16);
			if (ok) {
				switch (params.type) {
				case mCORE_MEMORY_SEARCH_8:
					ok = v < 0x100;
					params.value8 = v;
					break;
				case mCORE_MEMORY_SEARCH_16:
					ok = v < 0x10000;
					params.value16 = v;
					break;
				case mCORE_MEMORY_SEARCH_32:
					params.value32 = v;
					break;
				default:
					ok = false;
				}
			}
			if (ok) {
				mCoreMemorySearch(core, &params, &res, 10000);
			}
		}
		if (m_ui.numDec->isChecked()) {
			bool ok;
			uint32_t v = m_ui.value->text().toUInt(&ok, 10);
			if (ok) {
				switch (params.type) {
				case mCORE_MEMORY_SEARCH_8:
					ok = v < 0x100;
					params.value8 = v;
					break;
				case mCORE_MEMORY_SEARCH_16:
					ok = v < 0x10000;
					params.value16 = v;
					break;
				case mCORE_MEMORY_SEARCH_32:
					params.value32 = v;
					break;
				default:
					ok = false;
				}
			}
			if (ok) {
				mCoreMemorySearch(core, &params, &res, 10000);
			}
		}
	}
	if (m_ui.typeStr->isChecked()) {
		params.type = mCORE_MEMORY_SEARCH_STRING;
		string = m_ui.value->text().toLocal8Bit();
		params.valueStr = string;
		mCoreMemorySearch(core, &params, &res, 10000);
	}

	m_ui.results->clear();
	m_ui.results->setRowCount(mCoreMemorySearchResultsSize(&res));
	for (size_t i = 0; i < mCoreMemorySearchResultsSize(&res); ++i) {
		mCoreMemorySearchResult* result = mCoreMemorySearchResultsGetPointer(&res, i);
		QTableWidgetItem* item = new QTableWidgetItem(QString("%1").arg(result->address, 8, 16, QChar('0')));
		m_ui.results->setItem(i, 0, item);
		switch (result->type) {
		case mCORE_MEMORY_SEARCH_8:
			item = new QTableWidgetItem(QString("%1").arg(core->rawRead8(core, result->address, -1), 2, 16, QChar('0')));
			break;
		case mCORE_MEMORY_SEARCH_16:
			item = new QTableWidgetItem(QString("%1").arg(core->rawRead16(core, result->address, -1), 4, 16, QChar('0')));
			break;
		case mCORE_MEMORY_SEARCH_GUESS:
		case mCORE_MEMORY_SEARCH_32:
			item = new QTableWidgetItem(QString("%1").arg(core->rawRead32(core, result->address, -1), 8, 16, QChar('0')));
			break;
		case mCORE_MEMORY_SEARCH_STRING:
			item = new QTableWidgetItem(params.valueStr); // TODO
		}
		m_ui.results->setItem(i, 1, item);
	}
	m_ui.results->sortItems(0);

	mCoreMemorySearchResultsDeinit(&res);
}
