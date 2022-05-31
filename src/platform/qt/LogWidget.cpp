/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "LogWidget.h"

#include "GBAApp.h"

#include <QScrollBar>

using namespace QGBA;

LogWidget::LogWidget(QWidget* parent)
	: QTextEdit(parent)
{
	setFont(GBAApp::app()->monospaceFont());
}

void LogWidget::log(const QString& line) {
	moveCursor(QTextCursor::End);
	insertPlainText(line);
	verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}
