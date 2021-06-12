/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "LogView.h"

#include "LogController.h"
#include "Window.h"

#include <QTextBlock>
#include <QTextCursor>

using namespace QGBA;

LogView::LogView(LogController* log, Window* window, QWidget* parent)
	: QWidget(parent)
{
	m_ui.setupUi(this);
	connect(m_ui.levelDebug, &QAbstractButton::toggled, [this](bool set) {
		setLevel(mLOG_DEBUG, set);
	});
	connect(m_ui.levelStub, &QAbstractButton::toggled, [this](bool set) {
		setLevel(mLOG_STUB, set);
	});
	connect(m_ui.levelInfo, &QAbstractButton::toggled, [this](bool set) {
		setLevel(mLOG_INFO, set);
	});
	connect(m_ui.levelWarn, &QAbstractButton::toggled, [this](bool set) {
		setLevel(mLOG_WARN, set);
	});
	connect(m_ui.levelError, &QAbstractButton::toggled, [this](bool set) {
		setLevel(mLOG_ERROR, set);
	});
	connect(m_ui.levelFatal, &QAbstractButton::toggled, [this](bool set) {
		setLevel(mLOG_FATAL, set);
	});
	connect(m_ui.levelGameError, &QAbstractButton::toggled, [this](bool set) {
		setLevel(mLOG_GAME_ERROR, set);
	});
	connect(m_ui.clear, &QAbstractButton::clicked, this, &LogView::clear);
	connect(m_ui.advanced, &QAbstractButton::clicked, this, [window]() {
		window->openSettingsWindow(SettingsView::Page::LOGGING);
	});
	connect(m_ui.maxLines, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
	        this, &LogView::setMaxLines);
	m_ui.maxLines->setValue(DEFAULT_LINE_LIMIT);

	connect(log, &LogController::logPosted, this, &LogView::postLog);
	connect(log, static_cast<void (LogController::*)(int)>(&LogController::levelsSet), this, &LogView::setLevels);
	connect(log, static_cast<void (LogController::*)(int)>(&LogController::levelsEnabled), [this](int level) {
		bool s = blockSignals(true);
		setLevel(level, true);
		blockSignals(s);
	});
	connect(log, static_cast<void (LogController::*)(int)>(&LogController::levelsDisabled), [this](int level) {
		bool s = blockSignals(true);
		setLevel(level, false);
		blockSignals(s);
	});
	connect(this, &LogView::levelsEnabled, log, static_cast<void (LogController::*)(int)>(&LogController::enableLevels));
	connect(this, &LogView::levelsDisabled, log, static_cast<void (LogController::*)(int)>(&LogController::disableLevels));
}

void LogView::postLog(int level, int category, const QString& log) {
	QString line = QString("[%1] %2:\t%3").arg(LogController::toString(level)).arg(mLogCategoryName(category)).arg(log);
	// TODO: Log to file
	m_pendingLines.enqueue(line);
	++m_lines;
	if (m_lines > m_lineLimit) {
		clearLine();
	}
	update();
}

void LogView::clear() {
	m_ui.view->clear();
	m_lines = 0;
}

void LogView::setLevels(int levels) {
	m_ui.levelDebug->setCheckState(levels & mLOG_DEBUG ? Qt::Checked : Qt::Unchecked);
	m_ui.levelStub->setCheckState(levels & mLOG_STUB ? Qt::Checked : Qt::Unchecked);
	m_ui.levelInfo->setCheckState(levels & mLOG_INFO ? Qt::Checked : Qt::Unchecked);
	m_ui.levelWarn->setCheckState(levels & mLOG_WARN ? Qt::Checked : Qt::Unchecked);
	m_ui.levelError->setCheckState(levels & mLOG_ERROR ? Qt::Checked : Qt::Unchecked);
	m_ui.levelFatal->setCheckState(levels & mLOG_FATAL ? Qt::Checked : Qt::Unchecked);
	m_ui.levelGameError->setCheckState(levels & mLOG_GAME_ERROR ? Qt::Checked : Qt::Unchecked);
}

void LogView::setLevel(int level, bool set) {
	if (level & mLOG_DEBUG) {
		m_ui.levelDebug->setCheckState(set ? Qt::Checked : Qt::Unchecked);
	}
	if (level & mLOG_STUB) {
		m_ui.levelStub->setCheckState(set ? Qt::Checked : Qt::Unchecked);
	}
	if (level & mLOG_INFO) {
		m_ui.levelInfo->setCheckState(set ? Qt::Checked : Qt::Unchecked);
	}
	if (level & mLOG_WARN) {
		m_ui.levelWarn->setCheckState(set ? Qt::Checked : Qt::Unchecked);
	}
	if (level & mLOG_ERROR) {
		m_ui.levelError->setCheckState(set ? Qt::Checked : Qt::Unchecked);
	}
	if (level & mLOG_FATAL) {
		m_ui.levelFatal->setCheckState(set ? Qt::Checked : Qt::Unchecked);
	}
	if (level & mLOG_GAME_ERROR) {
		m_ui.levelGameError->setCheckState(set ? Qt::Checked : Qt::Unchecked);
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

void LogView::paintEvent(QPaintEvent* event) {
	while (!m_pendingLines.isEmpty()) {
		m_ui.view->appendPlainText(m_pendingLines.dequeue());
	}
	QWidget::paintEvent(event);
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
