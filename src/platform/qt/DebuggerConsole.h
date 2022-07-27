/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "ui_DebuggerConsole.h"

namespace QGBA {

class DebuggerConsoleController;

class DebuggerConsole : public QWidget {
Q_OBJECT

public:
	DebuggerConsole(DebuggerConsoleController* controller, QWidget* parent = nullptr);

private slots:
	void postLine();

protected:
	bool eventFilter(QObject*, QEvent*) override;

private:
	Ui::DebuggerConsole m_ui;
	int m_historyOffset;

	DebuggerConsoleController* m_consoleController;
};

}
