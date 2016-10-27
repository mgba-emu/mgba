/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "DebuggerREPL.h"

#include "DebuggerREPLController.h"

#include <QScrollBar>

using namespace QGBA;

DebuggerREPL::DebuggerREPL(DebuggerREPLController* controller, QWidget* parent)
	: QWidget(parent)
	, m_replController(controller)
{
	m_ui.setupUi(this);

	connect(m_ui.prompt, SIGNAL(returnPressed()), this, SLOT(postLine()));
	connect(controller, SIGNAL(log(const QString&)), this, SLOT(log(const QString&)));
	connect(m_ui.breakpoint, SIGNAL(clicked()), controller, SLOT(breakInto()));

	controller->attach();
}

void DebuggerREPL::log(const QString& line) {
	m_ui.log->moveCursor(QTextCursor::End);
	m_ui.log->insertPlainText(line);
	m_ui.log->verticalScrollBar()->setValue(m_ui.log->verticalScrollBar()->maximum());
}

void DebuggerREPL::postLine() {
	QString line = m_ui.prompt->text();
	m_ui.prompt->clear();
	if (line.isEmpty()) {
		m_replController->enterLine(QString("\n"));
	} else {
		log(QString("> %1\n").arg(line));
		m_replController->enterLine(line);
	}
}
