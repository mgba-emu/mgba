/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "DebuggerConsole.h"

#include "DebuggerConsoleController.h"

#include <QScrollBar>

using namespace QGBA;

DebuggerConsole::DebuggerConsole(DebuggerConsoleController* controller, QWidget* parent)
	: QWidget(parent)
	, m_consoleController(controller)
{
	m_ui.setupUi(this);

	connect(m_ui.prompt, SIGNAL(returnPressed()), this, SLOT(postLine()));
	connect(controller, SIGNAL(log(const QString&)), this, SLOT(log(const QString&)));
	connect(m_ui.breakpoint, SIGNAL(clicked()), controller, SLOT(attach()));
	connect(m_ui.breakpoint, SIGNAL(clicked()), controller, SLOT(breakInto()));
}

void DebuggerConsole::log(const QString& line) {
	m_ui.log->moveCursor(QTextCursor::End);
	m_ui.log->insertPlainText(line);
	m_ui.log->verticalScrollBar()->setValue(m_ui.log->verticalScrollBar()->maximum());
}

void DebuggerConsole::postLine() {
	m_consoleController->attach();
	QString line = m_ui.prompt->text();
	m_ui.prompt->clear();
	if (line.isEmpty()) {
		m_consoleController->enterLine(QString("\n"));
	} else {
		log(QString("> %1\n").arg(line));
		m_consoleController->enterLine(line);
	}
}
