/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "DebuggerConsole.h"

#include "DebuggerConsoleController.h"
#include "GBAApp.h"

#include <QKeyEvent>

using namespace QGBA;

DebuggerConsole::DebuggerConsole(DebuggerConsoleController* controller, QWidget* parent)
	: QWidget(parent)
	, m_consoleController(controller)
{
	m_ui.setupUi(this);

	m_ui.prompt->installEventFilter(this);
	m_ui.prompt->setFont(GBAApp::app()->monospaceFont());

	connect(m_ui.prompt, &QLineEdit::returnPressed, this, &DebuggerConsole::postLine);
	connect(controller, &DebuggerConsoleController::log, m_ui.log, &LogWidget::log);
	connect(m_ui.breakpoint, &QAbstractButton::clicked, controller, &DebuggerController::attach);
	connect(m_ui.breakpoint, &QAbstractButton::clicked, controller, &DebuggerController::breakInto);

	controller->historyLoad();
}

void DebuggerConsole::postLine() {
	m_consoleController->attach();
	QString line = m_ui.prompt->text();
	m_ui.prompt->clear();
	if (line.isEmpty()) {
		m_consoleController->enterLine(QString("\n"));
	} else {
		m_historyOffset = 0;
		m_ui.log->log(QString("> %1\n").arg(line));
		m_consoleController->enterLine(line);
	}
}

bool DebuggerConsole::eventFilter(QObject*, QEvent* event) {
	if (event->type() != QEvent::KeyPress) {
		return false;
	}
	QStringList history = m_consoleController->history();
	if (history.isEmpty()) {
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
		if (m_historyOffset >= history.size()) {
			return false;
		}
		++m_historyOffset;
		break;
	case Qt::Key_End:
		m_historyOffset = 0;
		break;
	case Qt::Key_Home:
		m_historyOffset = history.size();
		break;
	default:
		return false;
	}
	if (m_historyOffset == 0) {
		m_ui.prompt->clear();
	} else {
		m_ui.prompt->setText(history[history.size() - m_historyOffset]);
	}
	return true;
}
