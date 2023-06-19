/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <climits>
#include <QTextCodec>

#include "MemorySearch.h"

#include <mgba/core/core.h>

#include "CoreController.h"

using namespace QGBA;


MemorySearch::MemorySearch(std::shared_ptr<CoreController> controller, QWidget* parent)
	: QWidget(parent)
	, m_controller(controller)
{
	m_ui.setupUi(this);
	memView = nullptr;
	mCoreMemorySearchResultsInit(&m_results, 0);
	connect(m_ui.search, &QPushButton::clicked, this, &MemorySearch::search);
	connect(m_ui.value, &QLineEdit::returnPressed, this, &MemorySearch::search); 
	connect(m_ui.searchWithin, &QPushButton::clicked, this, &MemorySearch::searchWithin);
	connect(m_ui.numHex, &QPushButton::clicked, this, &MemorySearch::refresh);
	connect(m_ui.numDec, &QPushButton::clicked, this, &MemorySearch::refresh);
	connect(m_ui.results, &QTableWidget::cellDoubleClicked, this, &openMemory);

	connect(controller.get(), &CoreController::stopping, this, &QWidget::close);
}

MemorySearch::~MemorySearch() {
	mCoreMemorySearchResultsDeinit(&m_results);
}

bool MemorySearch::createParams(mCoreMemorySearchParams* params, QByteArray &string) {
	params->memoryFlags = mCORE_MEMORY_WRITE;
	if (m_ui.searchROM->isChecked()) {
		params->memoryFlags |= mCORE_MEMORY_READ;
	}
		
	bool ok = false;

	params->start = 0x2000000;
	uint32_t s = m_ui.start->text().toUInt(&ok, 16);
	if (ok && m_ui.start->text() != "") {
		params->start = s;
	}
	if (params->start < 0x2000000) {
		params->start = 0x2000000;
	}
	if (params->start > 0xA000000) {
		params->start = 0xA000000;
	}
	params->end = 0x7000400;
	uint32_t e = m_ui.end->text().toUInt(&ok, 16);
	if (ok && m_ui.end->text() != "") {
		params->end = e;
	} else {
		if (m_ui.searchROM->isChecked()) {
			params->end = 0xA000000;
		}
	}
	if (params->end < 0x2000000) {
		params->end = 0x3008000;  
	}
	if (params->end > 0xA000000) {
		params->end = 0xA000000;
	}
	QString str_addr;
	str_addr = QString("%1").arg(params->start, 0, 16);
	m_ui.label_s->setText(str_addr);
	str_addr = QString("%1").arg(params->end, 0, 16);
	m_ui.label_e->setText(str_addr);
	
	params->align = -1;
	params->width = 0;
	params->signedNum = false;

	if (m_ui.bits8->isChecked()) {
		params->width |= 1;
	}
	if (m_ui.bits16->isChecked()) {
		params->width |= 2;
	}
	if (m_ui.bits32->isChecked()) {
		params->width |= 4;
	}
	if (m_ui.typeNum->isChecked()) {
		params->type = mCORE_MEMORY_SEARCH_INT;
		if (m_ui.opChangedBy->isChecked()) {
			params->op = mCORE_MEMORY_SEARCH_CHANGED_BY;
		} else if (m_ui.opChanged->isChecked()) {
			params->op = m_ui.opNot->isChecked() ? mCORE_MEMORY_SEARCH_NOT_CHANGED : mCORE_MEMORY_SEARCH_CHANGED;
		} else if (m_ui.opIncBy->isChecked()) {
			params->op = mCORE_MEMORY_SEARCH_INCREASE_BY;
		} else if (m_ui.opInc->isChecked()) {
			params->op = m_ui.opNot->isChecked() ? mCORE_MEMORY_SEARCH_NOT_INCREASE : mCORE_MEMORY_SEARCH_INCREASE;
		} else if (m_ui.opDecBy->isChecked()) {
			params->op = mCORE_MEMORY_SEARCH_DECREASE_BY;
		} else if (m_ui.opDec->isChecked()) {
			params->op = m_ui.opNot->isChecked() ? mCORE_MEMORY_SEARCH_NOT_DECREASE : mCORE_MEMORY_SEARCH_DECREASE;
		} else if (m_ui.opUnknown->isChecked()) {
			params->op = mCORE_MEMORY_SEARCH_ANY;
		} else if (m_ui.opGreater->isChecked()) {
			params->op = m_ui.opNot->isChecked() ? mCORE_MEMORY_SEARCH_NOT_GREATER : mCORE_MEMORY_SEARCH_GREATER;
		} else if (m_ui.opLess->isChecked()) {
			params->op = m_ui.opNot->isChecked() ? mCORE_MEMORY_SEARCH_NOT_LESS: mCORE_MEMORY_SEARCH_LESS;
		} else {
			params->op = m_ui.opNot->isChecked() ? mCORE_MEMORY_SEARCH_NOT_EQUAL : mCORE_MEMORY_SEARCH_EQUAL;
		}
		
		if (m_ui.numHex->isChecked() && m_ui.value->text() != "") {
			uint32_t v = m_ui.value->text().toULong(&ok, 16);
			if (ok) {
				params->valueInt = v;
				if (v > UCHAR_MAX) {
					params->width &= 0x6;
				}
				if (v > USHRT_MAX) {
					params->width &= 0x4;
				}
			}
		}
	
		else if (m_ui.numDec->isChecked() && m_ui.value->text() != "") {
			int64_t v;
			if (m_ui.value->text().indexOf('-') > -1) {
				v = m_ui.value->text().toLong(&ok, 10);
				params->signedNum = true;
			} else {
				v = m_ui.value->text().toULong(&ok, 10);
			}
			if (ok) {
				params->valueInt = v;
				 if (params->signedNum) {
					if (v > SCHAR_MAX || v < SCHAR_MIN) {
						params->width &= 0x6;
					}
					if (v > SHRT_MAX || v < SHRT_MIN) {
						params->width &= 0x4;
					}
				 } else {
					if (v > UCHAR_MAX) {
						params->width &= 0x6;
					}
					if (v > USHRT_MAX) {
						params->width &= 0x4;
					}
				 }
			}
		}
		if (!ok) {
			switch (params->op) {
			case mCORE_MEMORY_SEARCH_INCREASE:
			case mCORE_MEMORY_SEARCH_NOT_INCREASE:
			case mCORE_MEMORY_SEARCH_DECREASE:
			case mCORE_MEMORY_SEARCH_NOT_DECREASE:
			case mCORE_MEMORY_SEARCH_CHANGED:
			case mCORE_MEMORY_SEARCH_NOT_CHANGED:
			case mCORE_MEMORY_SEARCH_ANY:
				 ok = params->width;
				 break;
			case mCORE_MEMORY_SEARCH_GREATER:
			case mCORE_MEMORY_SEARCH_NOT_GREATER:
			case mCORE_MEMORY_SEARCH_LESS:
			case mCORE_MEMORY_SEARCH_NOT_LESS:
			case mCORE_MEMORY_SEARCH_EQUAL:
			case mCORE_MEMORY_SEARCH_NOT_EQUAL:
			case mCORE_MEMORY_SEARCH_CHANGED_BY:
			case mCORE_MEMORY_SEARCH_INCREASE_BY:
			case mCORE_MEMORY_SEARCH_DECREASE_BY:
			default:
				 break;
			}
		}
	}
	else if (m_ui.typeByteArr->isChecked()) {
		params->type = mCORE_MEMORY_SEARCH_STRING;
		QString str = m_ui.value->text().simplified().remove(' ');
		if (str.length() % 2 != 0) {
			ok = false;
		}
		string = QByteArray::fromHex(str.toLatin1());
		params->valueStr = string.constData();
		params->width = string.size();
		ok = true;
	} else {
		params->type = mCORE_MEMORY_SEARCH_STRING;
		QString enc = m_ui.encoding->currentText();
		if (enc == "ASCII") {
			string = m_ui.value->text().toLatin1();
		} else {
			QTextCodec* codec = QTextCodec::codecForName(enc.toLatin1());
			string = codec->fromUnicode(m_ui.value->text());
		}
		params->valueStr = string.constData();
		params->width = string.size();
		ok = true;
	}

	m_ui.msg->setText(ok ? QString("MSG:") : QString("MSG: Invalid input parameters. Skip searching."));
	return ok;
}

void MemorySearch::search() {
	mCoreMemorySearchResultsClear(&m_results);
	mCoreMemorySearchParams params;
	QByteArray string;
	CoreController::Interrupter interrupter(m_controller);
	mCore* core = m_controller->thread()->core;

	if (createParams(&params, string)) {
		mCoreMemorySearch(core, &params, &m_results, LIMIT);
		if (mCoreMemorySearchResultsSize(&m_results) > 0) {
			m_ui.opChangedBy->setEnabled(true);
			m_ui.opChanged->setEnabled(true);
			m_ui.opDec->setEnabled(true);
			m_ui.opInc->setEnabled(true);
			m_ui.opDecBy->setEnabled(true);
			m_ui.opIncBy->setEnabled(true);
		}
		refresh();
	}	
}

void MemorySearch::searchWithin() {
	mCoreMemorySearchParams params;
	QByteArray string;
	CoreController::Interrupter interrupter(m_controller);
	mCore* core = m_controller->thread()->core;

	if (createParams(&params, string)) {
		mCoreMemorySearchRepeat(core, &params, &m_results);
		refresh();
	}
}

void MemorySearch::refresh() {
	CoreController::Interrupter interrupter(m_controller);
	mCore* core = m_controller->thread()->core;

	m_ui.results->clearContents();
	size_t resSize = mCoreMemorySearchResultsSize(&m_results);
	size_t dispCount = m_ui.dispAll->isChecked() ? resSize : (resSize > DISP_LIMIT ? DISP_LIMIT : resSize);
	m_ui.results->setRowCount(dispCount);
	for (size_t i = 0; i < dispCount; ++i) {
		mCoreMemorySearchResult* result = mCoreMemorySearchResultsGetPointer(&m_results, i);
		QTableWidgetItem* item = new QTableWidgetItem(QString("%1").arg(result->address, 8, 16, QChar('0')));
		m_ui.results->setItem(i, 0, item);
		QTableWidgetItem* prev = nullptr;
		QTableWidgetItem* type = nullptr;
		QByteArray string;
		if (result->type == mCORE_MEMORY_SEARCH_INT) {
			if (m_ui.numHex->isChecked()) {
				 item = new QTableWidgetItem(
				     QString("%1").arg(result->curValue, result->width << 1, 16, QChar('0')).right(result->width << 1));
				 prev = new QTableWidgetItem(
				     QString("%1").arg(result->oldValue, result->width << 1, 16, QChar('0')).right(result->width << 1));
			} else {
				 item = new QTableWidgetItem(QString("%1").arg(result->curValue, 0, 10));
				 prev = new QTableWidgetItem(QString("%1").arg(result->oldValue, 0, 10));
			}
		} else {
			string.reserve(result->width);
			for (int i = 0; i < result->width; ++i) {
				 string.append(core->rawRead8(core, result->address + i, result->segment));
			}
			item = new QTableWidgetItem(QString(string.toHex())); // TODO

			Q_ASSERT(item);
		}
		switch (result->type) {
		case mCORE_MEMORY_SEARCH_INT:
			type = new QTableWidgetItem(tr("%1 byte(s)").arg(result->width));
			break;
		case mCORE_MEMORY_SEARCH_STRING:
			type = new QTableWidgetItem("string");
			prev = new QTableWidgetItem("");
			break;
		}
		Q_ASSERT(type);

		m_ui.results->setItem(i, 1, item);
		m_ui.results->setItem(i, 2, prev);
		m_ui.results->setItem(i, 3, type);
	}
	m_ui.results->sortItems(0);
	m_ui.msg->setText(QString("MSG: display %1 of %2 results").arg(dispCount).arg(resSize));
}

void MemorySearch::openMemory(int row, int col) {
	
	uint32_t address = m_ui.results->item(row, 0)->text().toUInt(nullptr, 16);

	if (memView == nullptr) {
		memView = new MemoryView(m_controller);
	}

	memView->jumpToAddress(address);
	memView->show();
}
