/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_DEBUGGER_CONSOLE_CONTROLLER
#define QGBA_DEBUGGER_CONSOLE_CONTROLLER

#include "DebuggerController.h"

#include <QMutex>
#include <QStringList>
#include <QWaitCondition>

#include <mgba/internal/debugger/cli-debugger.h>

namespace QGBA {

class GameController;

class DebuggerConsoleController : public DebuggerController {
Q_OBJECT

public:
	DebuggerConsoleController(GameController* controller, QObject* parent = nullptr);

signals:
	void log(const QString&);
	void lineAppend(const QString&);

public slots:
	void enterLine(const QString&);
	virtual void detach() override;

protected:
	virtual void attachInternal() override;

private:
	static void printf(struct CLIDebuggerBackend* be, const char* fmt, ...);
	static void init(struct CLIDebuggerBackend* be);
	static void deinit(struct CLIDebuggerBackend* be);
	static const char* readLine(struct CLIDebuggerBackend* be, size_t* len);
	static void lineAppend(struct CLIDebuggerBackend* be, const char* line);
	static const char* historyLast(struct CLIDebuggerBackend* be, size_t* len);
	static void historyAppend(struct CLIDebuggerBackend* be, const char* line);

	CLIDebugger m_cliDebugger;

	QMutex m_mutex;
	QWaitCondition m_cond;
	QStringList m_history;
	QStringList m_lines;
	QByteArray m_last;

	struct Backend {
		CLIDebuggerBackend d;
		DebuggerConsoleController* self;
	} m_backend;
};

}

#endif
