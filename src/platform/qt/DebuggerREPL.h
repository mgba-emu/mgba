/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_DEBUGGER_REPL
#define QGBA_DEBUGGER_REPL

#include "ui_DebuggerREPL.h"

namespace QGBA {

class DebuggerREPLController;

class DebuggerREPL : public QWidget {
Q_OBJECT

public:
	DebuggerREPL(DebuggerREPLController* controller, QWidget* parent = nullptr);

private slots:
	void log(const QString&);
	void postLine();

private:
	Ui::DebuggerREPL m_ui;

	DebuggerREPLController* m_replController;
};

}

#endif
