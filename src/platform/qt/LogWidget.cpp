/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "LogWidget.h"

#include "GBAApp.h"

#include <QScrollBar>

using namespace QGBA;

QTextCharFormat LogWidget::s_warn;
QTextCharFormat LogWidget::s_error;
QTextCharFormat LogWidget::s_prompt;

LogWidget::LogWidget(QWidget* parent)
	: QPlainTextEdit(parent)
{
	setFont(GBAApp::app()->monospaceFont());

	QPalette palette = QApplication::palette();
	s_warn.setFontWeight(QFont::DemiBold);
	s_warn.setFontItalic(true);
	s_warn.setForeground(Qt::yellow);
	s_warn.setBackground(QColor(255, 255, 0, 64));
	s_error.setFontWeight(QFont::Bold);
	s_error.setForeground(Qt::red);
	s_error.setBackground(QColor(255, 0, 0, 64));
	s_prompt.setForeground(palette.brush(QPalette::Disabled, QPalette::Text));
}

void LogWidget::log(const QString& line) {
	moveCursor(QTextCursor::End);
	textCursor().insertText(line, {});
	if (m_newlineTerminated) {
		textCursor().insertText("\n");
	}
	verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void LogWidget::warn(const QString& line) {
	moveCursor(QTextCursor::End);
	textCursor().insertText(WARN_PREFIX + line, s_warn);
	if (m_newlineTerminated) {
		textCursor().insertText("\n");
	}
	verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void LogWidget::error(const QString& line) {
	moveCursor(QTextCursor::End);
	textCursor().insertText(ERROR_PREFIX + line, s_error);
	if (m_newlineTerminated) {
		textCursor().insertText("\n");
	}
	verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void LogWidget::echo(const QString& line) {
	moveCursor(QTextCursor::End);
	textCursor().insertText(PROMPT_PREFIX + line, s_prompt);
	if (m_newlineTerminated) {
		textCursor().insertText("\n");
	}
	verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}
