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

	m_ui.prompt->installEventFilter(this);

	connect(m_ui.prompt, &QLineEdit::returnPressed, this, &DebuggerConsole::postLine);
	connect(controller, &DebuggerConsoleController::log, this, &DebuggerConsole::log);
	connect(m_ui.breakpoint, &QAbstractButton::clicked, controller, &DebuggerController::attach);
	connect(m_ui.breakpoint, &QAbstractButton::clicked, controller, &DebuggerController::breakInto);
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
		m_history.append(line);
		m_historyOffset = 0;
		log(QString("> %1\n").arg(line));
		m_consoleController->enterLine(line);
	}
}

bool DebuggerConsole::eventFilter(QObject*, QEvent* event) {
	if (event->type() != QEvent::KeyPress) {
		return false;
	}
	if (m_history.isEmpty()) {
		return false;
	}
	QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
	switch (keyEvent->key()) {
	case Qt::Key_Down:
		if (m_historyOffset <= 0) {
			return false;
		}
		--m_historyOffset;
		break;
	case Qt::Key_Up:
		if (m_historyOffset >= m_history.size()) {
			return false;
		}
		++m_historyOffset;
		break;
	case Qt::Key_End:
		m_historyOffset = 0;
		break;
	case Qt::Key_Home:
		m_historyOffset = m_history.size();
		break;
	default:
		return false;
	}
	if (m_historyOffset == 0) {
		m_ui.prompt->clear();
	} else {
		m_ui.prompt->setText(m_history[m_history.size() - m_historyOffset]);
	}
	return true;
}