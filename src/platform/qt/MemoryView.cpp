/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <QTextCodec>

#include "MemoryView.h"

#include "CoreController.h"
#include "MemoryDump.h"

#include <mgba/core/core.h>

using namespace QGBA;

IntValidator::IntValidator(bool isSigned, QObject* parent)
	: QValidator(parent)
	, m_signed(isSigned)
{
}

QValidator::State IntValidator::validate(QString& input, int&) const {
	if (input.isEmpty()) {
		return QValidator::Intermediate;
	}
	if (input.size() == 1 && input[0] == '-') {
		if (m_signed) {
			return QValidator::Intermediate;
		} else {
			return QValidator::Invalid;
		}
	}
	if (input[0].isSpace()) {
		return QValidator::Invalid;
	}
	if (input[input.size() - 1].isSpace()) {
		return QValidator::Invalid;
	}
	if (input.size() > 1 && input[0] == '0') {
		return QValidator::Invalid;
	}

	bool ok = false;
	qlonglong val = locale().toLongLong(input, &ok);
	if (!ok) {
		return QValidator::Invalid;
	}

	qlonglong hardmax;
	qlonglong hardmin;
	qlonglong max;
	qlonglong min;

	if (m_signed) {
		switch (m_width) {
		case 1:
			hardmax = 999LL;
			hardmin = -999LL;
			max = 0x7FLL;
			min = -0x80LL;
			break;
		case 2:
			hardmax = 99999LL;
			hardmin = -99999LL;
			max = 0x7FFFLL;
			min = -0x8000LL;
			break;
		case 4:
			hardmax = 9999999999LL;
			hardmin = -9999999999LL;
			max = 0x7FFFFFFFLL;
			min = -0x80000000LL;
			break;
		default:
			return QValidator::Invalid;
		}
	} else {
		hardmin = 0;
		min = 0;

		switch (m_width) {
		case 1:
			hardmax = 999LL;
			max = 0xFFLL;
			break;
		case 2:
			hardmax = 99999LL;
			max = 0xFFFFLL;
			break;
		case 4:
			hardmax = 9999999999LL;
			max = 0xFFFFFFFFLL;
			break;
		default:
			return QValidator::Invalid;
		}
	}
	if (val < hardmin || val > hardmax) {
		return QValidator::Invalid;
	}
	if (val < min || val > max) {
		return QValidator::Intermediate;
	}
	return QValidator::Acceptable;
}

MemoryView::MemoryView(std::shared_ptr<CoreController> controller, QWidget* parent)
    : QWidget(parent)
    , m_controller(controller) {
	m_ui.setupUi(this);

	m_ui.hexfield->setController(controller);

	mCore* core = m_controller->thread()->core;
	const mCoreMemoryBlock* info;
	size_t nBlocks = core->listMemoryBlocks(core, &info);

	connect(m_ui.regions, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
	        &MemoryView::setIndex);
	connect(m_ui.segments, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
	        &MemoryView::setSegment);

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
	connect(m_ui.setAddress, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
	        static_cast<void (MemoryView::*)(uint32_t)>(&MemoryView::jumpToAddress));
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
	if (index < 0 || static_cast<size_t>(index) >= nBlocks) {
		return;
	}
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
	int index = m_ui.regions->currentIndex();
	if (index < 0 || static_cast<size_t>(index) >= nBlocks) {
		return;
	}
	const mCoreMemoryBlock& info = blocks[index];

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
	mCore* core = m_controller->thread()->core;
	QByteArray txt;
	uint8_t chr;
	int i = 0;
	do {
		chr = core->rawRead8(core, m_selection.first + i, m_ui.segments->value());
		txt.append(chr);
		++i;
	} while (chr);

	QString enc = m_ui.encoding->currentText();
	if (enc == "ASCII") {
		m_ui.stringVal->setText(QString::fromLatin1(txt.constData()));
	} else {
		QTextCodec* codec = QTextCodec::codecForName(enc.toLatin1());
		m_ui.stringVal->setText(codec->toUnicode(txt.constData()));
	}
		
	union {
		uint32_t u32;
		int32_t i32;
		uint16_t u16;
		int16_t i16;
		uint8_t u8;
		int8_t i8;
	} value;
	
	value.u32 = core->rawRead8(core, m_selection.first, m_ui.segments->value()) |
	    core->rawRead8(core, m_selection.first + 1, m_ui.segments->value()) << 8 |
	    core->rawRead8(core, m_selection.first + 2, m_ui.segments->value()) <<16 |
	    core->rawRead8(core, m_selection.first + 3, m_ui.segments->value()) << 24;
	
	m_ui.int8->setText(QString::number(value.i8));
	m_ui.uint8->setText(QString::number(value.u8));

	m_ui.int16->setText(QString::number(value.i16));
	m_ui.uint16->setText(QString::number(value.u16));

	m_ui.int32->setText(QString::number(value.i32));
	m_ui.uint32->setText(QString::number(value.u32));
	
}

void MemoryView::saveRange() {
	MemoryDump* memdump = new MemoryDump(m_controller);
	memdump->setAttribute(Qt::WA_DeleteOnClose);
	memdump->setAddress(m_selection.first);
	memdump->setSegment(m_ui.segments->value());
	memdump->setByteCount(m_selection.second - m_selection.first);
	memdump->show();
}
