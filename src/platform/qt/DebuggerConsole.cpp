/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "DebuggerConsole.h"
#include "moc_DebuggerConsole.cpp"

#include "DebuggerConsoleController.h"
#include "GBAApp.h"

#include <QKeyEvent>

using namespace QGBA;

DebuggerConsole::DebuggerConsole(DebuggerConsoleController* controller, QWidget* parent)
	: QWidget(parent)
	, m_consoleController(controller)
{
	m_ui.setupUi(this);

	m_ui.prompt->setFont(GBAApp::app()->monospaceFont());
	m_ui.prompt->setModel(m_consoleController->history());

	connect(m_ui.prompt, &HistoryLineEdit::linePosted, this, &DebuggerConsole::postLine);
	connect(m_ui.prompt, &HistoryLineEdit::emptyLinePosted, this, &DebuggerConsole::repeat);
	connect(controller, &DebuggerConsoleController::log, m_ui.log, &LogWidget::log);
	connect(controller, &DebuggerConsoleController::log, this, &DebuggerConsole::updatePaused);
	connect(m_ui.breakpoint, &QAbstractButton::clicked, controller, &DebuggerController::attach);
	connect(m_ui.breakpoint, &QAbstractButton::clicked, controller, &DebuggerController::breakInto);
	connect(m_ui.breakpoint, &QAbstractButton::clicked, this, &DebuggerConsole::updatePaused);
	connect(m_ui.doContinue, &QAbstractButton::clicked, controller, &DebuggerController::attach);
	connect(m_ui.doContinue, &QAbstractButton::clicked, controller, &DebuggerConsoleController::doContinue);
	connect(m_ui.doContinue, &QAbstractButton::clicked, this, &DebuggerConsole::updatePaused);

	controller->historyLoad();
}

void DebuggerConsole::postLine(const QString& line) {
	m_consoleController->attach();
	m_ui.log->log(QString("> %1\n").arg(line));
	m_consoleController->enterLine(line);
	updatePaused();
}

void DebuggerConsole::repeat() {
	m_consoleController->attach();
	m_consoleController->enterLine(QString("\n"));
	updatePaused();
}

void DebuggerConsole::updatePaused() {
	if (m_consoleController->isPaused()) {
		m_ui.breakpoint->setEnabled(false);
		m_ui.doContinue->setEnabled(true);
	} else {
		m_ui.breakpoint->setEnabled(true);
		m_ui.doContinue->setEnabled(false);
	}
}
