/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MemorySearch.h"

#include <mgba/core/core.h>

#include "CoreController.h"
#include "MemoryView.h"

using namespace QGBA;

MemorySearch::MemorySearch(std::shared_ptr<CoreController> controller, QWidget* parent)
	: QWidget(parent)
	, m_controller(controller)
{
	m_ui.setupUi(this);

	mCoreMemorySearchResultsInit(&m_results, 0);
	connect(m_ui.search, &QPushButton::clicked, this, &MemorySearch::search);
	connect(m_ui.searchWithin, &QPushButton::clicked, this, &MemorySearch::searchWithin);
	connect(m_ui.refresh, &QPushButton::clicked, this, &MemorySearch::refresh);
	connect(m_ui.numHex, &QPushButton::clicked, this, &MemorySearch::refresh);
	connect(m_ui.numDec, &QPushButton::clicked, this, &MemorySearch::refresh);
	connect(m_ui.viewMem, &QPushButton::clicked, this, &MemorySearch::openMemory);

	connect(controller.get(), &CoreController::stopping, this, &QWidget::close);
}

MemorySearch::~MemorySearch() {
	mCoreMemorySearchResultsDeinit(&m_results);
}

bool MemorySearch::createParams(mCoreMemorySearchParams* params) {
	params->memoryFlags = mCORE_MEMORY_RW;
	mCore* core = m_controller->thread()->core;

	QByteArray string;
	bool ok = false;
	if (m_ui.typeNum->isChecked()) {
		params->type = mCORE_MEMORY_SEARCH_INT;
		if (m_ui.opDelta->isChecked()) {
			params->op = mCORE_MEMORY_SEARCH_DELTA;
		} else if (m_ui.opGreater->isChecked()) {
			params->op = mCORE_MEMORY_SEARCH_GREATER;
		} else if (m_ui.opLess->isChecked()) {
			params->op = mCORE_MEMORY_SEARCH_LESS;
		} else {
			params->op = mCORE_MEMORY_SEARCH_EQUAL;
		}
		params->align = -1;
		if (m_ui.bits8->isChecked()) {
			params->width = 1;
		}
		if (m_ui.bits16->isChecked()) {
			params->width = 2;
		}
		if (m_ui.bits32->isChecked()) {
			params->width = 4;
		}
		if (m_ui.bitsGuess->isChecked()) {
			params->width = -1;
		}
		if (m_ui.numHex->isChecked()) {
			uint32_t v = m_ui.value->text().toUInt(&ok, 16);
			if (ok) {
				params->valueInt = v;
				switch (params->width) {
				case 1:
					ok = v < 0x100;
					break;
				case 2:
					ok = v < 0x10000;
					break;
				case 4:
					break;
				default:
					ok = false;
					break;
				}
			}
		}
		if (m_ui.numDec->isChecked()) {
			uint32_t v = m_ui.value->text().toUInt(&ok, 10);
			if (ok) {
				params->valueInt = v;
				switch (params->width) {
				case 1:
					ok = v < 0x100;
					break;
				case 2:
					ok = v < 0x10000;
					break;
				case 4:
					break;
				default:
					ok = false;
				}
			}
		}
		if (m_ui.numGuess->isChecked()) {
			params->type = mCORE_MEMORY_SEARCH_GUESS;
			m_string = m_ui.value->text().toLocal8Bit();
			params->valueStr = m_string.constData();
			ok = true;
		}
	}
	if (m_ui.typeStr->isChecked()) {
		params->type = mCORE_MEMORY_SEARCH_STRING;
		m_string = m_ui.value->text().toLocal8Bit();
		params->valueStr = m_string.constData();
		params->width = m_ui.value->text().size();
		ok = true;
	}
	return ok;
}

void MemorySearch::search() {
	mCoreMemorySearchResultsClear(&m_results);

	mCoreMemorySearchParams params;

	CoreController::Interrupter interrupter(m_controller);
	mCore* core = m_controller->thread()->core;

	if (createParams(&params)) {
		mCoreMemorySearch(core, &params, &m_results, LIMIT);
	}

	refresh();
}

void MemorySearch::searchWithin() {
	mCoreMemorySearchParams params;

	CoreController::Interrupter interrupter(m_controller);
	mCore* core = m_controller->thread()->core;

	if (createParams(&params)) {
		mCoreMemorySearchRepeat(core, &params, &m_results);
	}

	refresh();
}

void MemorySearch::refresh() {
	CoreController::Interrupter interrupter(m_controller);
	mCore* core = m_controller->thread()->core;

	m_ui.results->clearContents();
	m_ui.results->setRowCount(mCoreMemorySearchResultsSize(&m_results));
	m_ui.opDelta->setEnabled(false);
	for (size_t i = 0; i < mCoreMemorySearchResultsSize(&m_results); ++i) {
		mCoreMemorySearchResult* result = mCoreMemorySearchResultsGetPointer(&m_results, i);
		QTableWidgetItem* item = new QTableWidgetItem(QString("%1").arg(result->address, 8, 16, QChar('0')));
		m_ui.results->setItem(i, 0, item);
		QTableWidgetItem* type;
		QByteArray string;
		if (result->type == mCORE_MEMORY_SEARCH_INT && m_ui.numHex->isChecked()) {
			switch (result->width) {
			case 1:
				item = new QTableWidgetItem(QString("%1").arg(core->rawRead8(core, result->address, result->segment), 2, 16, QChar('0')));
				break;
			case 2:
				item = new QTableWidgetItem(QString("%1").arg(core->rawRead16(core, result->address, result->segment), 4, 16, QChar('0')));
				break;
			case 4:
				item = new QTableWidgetItem(QString("%1").arg(core->rawRead32(core, result->address, result->segment), 8, 16, QChar('0')));
				break;
			}
		} else {
			switch (result->type) {
			case mCORE_MEMORY_SEARCH_INT:
				switch (result->width) {
				case 1:
					item = new QTableWidgetItem(QString::number(core->rawRead8(core, result->address, result->segment)));
					break;
				case 2:
					item = new QTableWidgetItem(QString::number(core->rawRead16(core, result->address, result->segment)));
					break;
				case 4:
					item = new QTableWidgetItem(QString::number(core->rawRead32(core, result->address, result->segment)));
					break;
				}
				break;
			case mCORE_MEMORY_SEARCH_STRING:
				string.reserve(result->width);
				for (int i = 0; i < result->width; ++i) {
					string.append(core->rawRead8(core, result->address + i, result->segment));
				}
				item = new QTableWidgetItem(QLatin1String(string)); // TODO
			}
		}
		QString divisor;
		if (result->guessDivisor > 1) {
			if (result->guessMultiplier > 1) {
				divisor = tr(" (%0/%1×)").arg(result->guessMultiplier).arg(result->guessMultiplier);
			} else {
				divisor = tr(" (⅟%0×)").arg(result->guessDivisor);
			}
		} else if (result->guessMultiplier > 1) {
			divisor = tr(" (%0×)").arg(result->guessMultiplier);
		}
		switch (result->type) {
		case mCORE_MEMORY_SEARCH_INT:
			type = new QTableWidgetItem(tr("%1 byte%2").arg(result->width).arg(divisor));
			break;
		case mCORE_MEMORY_SEARCH_STRING:
			type = new QTableWidgetItem("string");
		}
		m_ui.results->setItem(i, 1, item);
		m_ui.results->setItem(i, 2, type);
		m_ui.opDelta->setEnabled(true);
	}
	if (m_ui.opDelta->isChecked() && !m_ui.opDelta->isEnabled()) {
		m_ui.opEqual->setChecked(true);
	}
	m_ui.results->sortItems(0);
}

void MemorySearch::openMemory() {
	auto items = m_ui.results->selectedItems();
	if (items.empty()) {
		return;
	}
	QTableWidgetItem* item = items[0];
	uint32_t address = item->text().toUInt(nullptr, 16);

	MemoryView* memView = new MemoryView(m_controller);
	memView->jumpToAddress(address);

	memView->setAttribute(Qt::WA_DeleteOnClose);
	memView->show();
}
