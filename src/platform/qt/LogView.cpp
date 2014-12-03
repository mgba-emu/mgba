/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "LogView.h"

#include <QTextBlock>
#include <QTextCursor>

using namespace QGBA;

LogView::LogView(QWidget* parent)
	: QWidget(parent)
{
	m_ui.setupUi(this);
	connect(m_ui.levelDebug, SIGNAL(toggled(bool)), this, SLOT(setLevelDebug(bool)));
	connect(m_ui.levelStub, SIGNAL(toggled(bool)), this, SLOT(setLevelStub(bool)));
	connect(m_ui.levelInfo, SIGNAL(toggled(bool)), this, SLOT(setLevelInfo(bool)));
	connect(m_ui.levelWarn, SIGNAL(toggled(bool)), this, SLOT(setLevelWarn(bool)));
	connect(m_ui.levelError, SIGNAL(toggled(bool)), this, SLOT(setLevelError(bool)));
	connect(m_ui.levelFatal, SIGNAL(toggled(bool)), this, SLOT(setLevelFatal(bool)));
	connect(m_ui.levelGameError, SIGNAL(toggled(bool)), this, SLOT(setLevelGameError(bool)));
	connect(m_ui.levelSWI, SIGNAL(toggled(bool)), this, SLOT(setLevelSWI(bool)));
	connect(m_ui.clear, SIGNAL(clicked()), this, SLOT(clear()));
	connect(m_ui.maxLines, SIGNAL(valueChanged(int)), this, SLOT(setMaxLines(int)));
	m_logLevel = 0;
	m_lines = 0;
	m_ui.maxLines->setValue(DEFAULT_LINE_LIMIT);
}

void LogView::postLog(int level, const QString& log) {
	if (!(level & m_logLevel)) {
		return;
	}
	m_ui.view->appendPlainText(QString("%1:\t%2").arg(toString(level)).arg(log));
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
	m_logLevel = levels;

	m_ui.levelDebug->setCheckState(levels & GBA_LOG_DEBUG ? Qt::Checked : Qt::Unchecked);
	m_ui.levelStub->setCheckState(levels & GBA_LOG_STUB ? Qt::Checked : Qt::Unchecked);
	m_ui.levelInfo->setCheckState(levels & GBA_LOG_INFO ? Qt::Checked : Qt::Unchecked);
	m_ui.levelWarn->setCheckState(levels & GBA_LOG_WARN ? Qt::Checked : Qt::Unchecked);
	m_ui.levelError->setCheckState(levels & GBA_LOG_ERROR ? Qt::Checked : Qt::Unchecked);
	m_ui.levelFatal->setCheckState(levels & GBA_LOG_FATAL ? Qt::Checked : Qt::Unchecked);
	m_ui.levelGameError->setCheckState(levels & GBA_LOG_GAME_ERROR ? Qt::Checked : Qt::Unchecked);
	m_ui.levelSWI->setCheckState(levels & GBA_LOG_SWI ? Qt::Checked : Qt::Unchecked);

	emit levelsSet(levels);
}

void LogView::setLevelDebug(bool set) {
	if (set) {
		setLevel(GBA_LOG_DEBUG);
	} else {
		clearLevel(GBA_LOG_DEBUG);
	}
}

void LogView::setLevelStub(bool set) {
	if (set) {
		setLevel(GBA_LOG_STUB);
	} else {
		clearLevel(GBA_LOG_STUB);
	}
}

void LogView::setLevelInfo(bool set) {
	if (set) {
		setLevel(GBA_LOG_INFO);
	} else {
		clearLevel(GBA_LOG_INFO);
	}
}

void LogView::setLevelWarn(bool set) {
	if (set) {
		setLevel(GBA_LOG_WARN);
	} else {
		clearLevel(GBA_LOG_WARN);
	}
}

void LogView::setLevelError(bool set) {
	if (set) {
		setLevel(GBA_LOG_ERROR);
	} else {
		clearLevel(GBA_LOG_ERROR);
	}
}

void LogView::setLevelFatal(bool set) {
	if (set) {
		setLevel(GBA_LOG_FATAL);
	} else {
		clearLevel(GBA_LOG_FATAL);
	}
}

void LogView::setLevelGameError(bool set) {
	if (set) {
		setLevel(GBA_LOG_GAME_ERROR);
	} else {
		clearLevel(GBA_LOG_GAME_ERROR);
	}
}

void LogView::setLevelSWI(bool set) {
	if (set) {
		setLevel(GBA_LOG_SWI);
	} else {
		clearLevel(GBA_LOG_SWI);
	}
}

void LogView::setMaxLines(int limit) {
	m_lineLimit = limit;
	while (m_lines > m_lineLimit) {
		clearLine();
	}
}

QString LogView::toString(int level) {
	switch (level) {
	case GBA_LOG_DEBUG:
		return tr("DEBUG");
	case GBA_LOG_STUB:
		return tr("STUB");
	case GBA_LOG_INFO:
		return tr("INFO");
	case GBA_LOG_WARN:
		return tr("WARN");
	case GBA_LOG_ERROR:
		return tr("ERROR");
	case GBA_LOG_FATAL:
		return tr("FATAL");
	case GBA_LOG_GAME_ERROR:
		return tr("GAME ERROR");
	case GBA_LOG_SWI:
		return tr("SWI");
	}
	return QString();
}

void LogView::setLevel(int level) {
	m_logLevel |= level;
	emit levelsEnabled(level);
}

void LogView::clearLevel(int level) {
	m_logLevel &= ~level;
	emit levelsDisabled(level);
}

void LogView::clearLine() {
	QTextCursor cursor(m_ui.view->document());
	cursor.setPosition(0);
	cursor.select(QTextCursor::BlockUnderCursor);
	cursor.removeSelectedText();
	cursor.deleteChar();
	--m_lines;
}
