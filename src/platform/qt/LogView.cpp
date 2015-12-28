/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "LogView.h"

#include "LogController.h"

#include <QTextBlock>
#include <QTextCursor>

using namespace QGBA;

LogView::LogView(LogController* log, QWidget* parent)
	: QWidget(parent)
	, m_lines(0)
	, m_lineLimit(DEFAULT_LINE_LIMIT)
{
	m_ui.setupUi(this);
	connect(m_ui.levelDebug, &QAbstractButton::toggled, [this](bool set) {
		setLevel(GBA_LOG_DEBUG, set);
	});
	connect(m_ui.levelStub, &QAbstractButton::toggled, [this](bool set) {
		setLevel(GBA_LOG_STUB, set);
	});
	connect(m_ui.levelInfo, &QAbstractButton::toggled, [this](bool set) {
		setLevel(GBA_LOG_INFO, set);
	});
	connect(m_ui.levelWarn, &QAbstractButton::toggled, [this](bool set) {
		setLevel(GBA_LOG_WARN, set);
	});
	connect(m_ui.levelError, &QAbstractButton::toggled, [this](bool set) {
		setLevel(GBA_LOG_ERROR, set);
	});
	connect(m_ui.levelFatal, &QAbstractButton::toggled, [this](bool set) {
		setLevel(GBA_LOG_FATAL, set);
	});
	connect(m_ui.levelGameError, &QAbstractButton::toggled, [this](bool set) {
		setLevel(GBA_LOG_GAME_ERROR, set);
	});
	connect(m_ui.levelSWI, &QAbstractButton::toggled, [this](bool set) {
		setLevel(GBA_LOG_SWI, set);
	});
	connect(m_ui.levelStatus, &QAbstractButton::toggled, [this](bool set) {
		setLevel(GBA_LOG_STATUS, set);
	});
	connect(m_ui.levelSIO, &QAbstractButton::toggled, [this](bool set) {
		setLevel(GBA_LOG_SIO, set);
	});
	connect(m_ui.clear, SIGNAL(clicked()), this, SLOT(clear()));
	connect(m_ui.maxLines, SIGNAL(valueChanged(int)), this, SLOT(setMaxLines(int)));
	m_ui.maxLines->setValue(DEFAULT_LINE_LIMIT);

	connect(log, SIGNAL(logPosted(int, const QString&)), this, SLOT(postLog(int, const QString&)));
	connect(log, SIGNAL(levelsSet(int)), this, SLOT(setLevels(int)));
	connect(log, &LogController::levelsEnabled, [this](int level) {
		bool s = blockSignals(true);
		setLevel(level, true);
		blockSignals(s);
	});
	connect(log, &LogController::levelsDisabled, [this](int level) {
		bool s = blockSignals(true);
		setLevel(level, false);
		blockSignals(s);
	});
	connect(this, SIGNAL(levelsEnabled(int)), log, SLOT(enableLevels(int)));
	connect(this, SIGNAL(levelsDisabled(int)), log, SLOT(disableLevels(int)));
}

void LogView::postLog(int level, const QString& log) {
	QString line = QString("%1:\t%2").arg(LogController::toString(level)).arg(log);
	if (isVisible()) {
		m_ui.view->appendPlainText(line);
	} else {
		m_pendingLines.enqueue(line);
	}
	++m_lines;
	if (m_lines > m_lineLimit) {
		clearLine();
	}
}

void LogView::clear() {
	m_ui.view->clear();
	m_lines = 0;
}

void LogView::setLevels(int levels) {
	m_ui.levelDebug->setCheckState(levels & GBA_LOG_DEBUG ? Qt::Checked : Qt::Unchecked);
	m_ui.levelStub->setCheckState(levels & GBA_LOG_STUB ? Qt::Checked : Qt::Unchecked);
	m_ui.levelInfo->setCheckState(levels & GBA_LOG_INFO ? Qt::Checked : Qt::Unchecked);
	m_ui.levelWarn->setCheckState(levels & GBA_LOG_WARN ? Qt::Checked : Qt::Unchecked);
	m_ui.levelError->setCheckState(levels & GBA_LOG_ERROR ? Qt::Checked : Qt::Unchecked);
	m_ui.levelFatal->setCheckState(levels & GBA_LOG_FATAL ? Qt::Checked : Qt::Unchecked);
	m_ui.levelGameError->setCheckState(levels & GBA_LOG_GAME_ERROR ? Qt::Checked : Qt::Unchecked);
	m_ui.levelSWI->setCheckState(levels & GBA_LOG_SWI ? Qt::Checked : Qt::Unchecked);
	m_ui.levelStatus->setCheckState(levels & GBA_LOG_STATUS ? Qt::Checked : Qt::Unchecked);
	m_ui.levelSIO->setCheckState(levels & GBA_LOG_SIO ? Qt::Checked : Qt::Unchecked);
}

void LogView::setLevel(int level, bool set) {
	if (level & GBA_LOG_DEBUG) {
		m_ui.levelDebug->setCheckState(set ? Qt::Checked : Qt::Unchecked);
	}
	if (level & GBA_LOG_STUB) {
		m_ui.levelStub->setCheckState(set ? Qt::Checked : Qt::Unchecked);
	}
	if (level & GBA_LOG_INFO) {
		m_ui.levelInfo->setCheckState(set ? Qt::Checked : Qt::Unchecked);
	}
	if (level & GBA_LOG_WARN) {
		m_ui.levelWarn->setCheckState(set ? Qt::Checked : Qt::Unchecked);
	}
	if (level & GBA_LOG_ERROR) {
		m_ui.levelError->setCheckState(set ? Qt::Checked : Qt::Unchecked);
	}
	if (level & GBA_LOG_FATAL) {
		m_ui.levelFatal->setCheckState(set ? Qt::Checked : Qt::Unchecked);
	}
	if (level & GBA_LOG_GAME_ERROR) {
		m_ui.levelGameError->setCheckState(set ? Qt::Checked : Qt::Unchecked);
	}
	if (level & GBA_LOG_SWI) {
		m_ui.levelSWI->setCheckState(set ? Qt::Checked : Qt::Unchecked);
	}
	if (level & GBA_LOG_STATUS) {
		m_ui.levelStatus->setCheckState(set ? Qt::Checked : Qt::Unchecked);
	}
	if (level & GBA_LOG_SIO) {
		m_ui.levelSIO->setCheckState(set ? Qt::Checked : Qt::Unchecked);
	}

	if (set) {
		emit levelsEnabled(level);
	} else {
		emit levelsDisabled(level);
	}
}

void LogView::setMaxLines(int limit) {
	m_lineLimit = limit;
	while (m_lines > m_lineLimit) {
		clearLine();
	}
}

void LogView::showEvent(QShowEvent*) {
	while (!m_pendingLines.isEmpty()) {
		m_ui.view->appendPlainText(m_pendingLines.dequeue());
	}
}

void LogView::clearLine() {
	if (m_ui.view->document()->isEmpty()) {
		m_pendingLines.dequeue();
	} else {
		QTextCursor cursor(m_ui.view->document());
		cursor.setPosition(0);
		cursor.select(QTextCursor::BlockUnderCursor);
		cursor.removeSelectedText();
		cursor.deleteChar();
	}
	--m_lines;
}
