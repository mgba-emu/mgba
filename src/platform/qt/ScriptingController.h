/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QHash>
#include <QObject>

#include <mgba/script/context.h>
#include <mgba/core/scripting.h>

#include "VFileDevice.h"

#include <memory>

namespace QGBA {

class CoreController;
class ScriptingTextBuffer;

class ScriptingController : public QObject {
Q_OBJECT

public:
	ScriptingController(QObject* parent = nullptr);
	~ScriptingController();

	void setController(std::shared_ptr<CoreController> controller);

	bool loadFile(const QString& path);
	bool load(VFileDevice& vf);

	mScriptContext* context() { return &m_scriptContext; }

signals:
	void log(const QString&);
	void warn(const QString&);
	void error(const QString&);
	void textBufferCreated(ScriptingTextBuffer*);

public slots:
	void clearController();
	void runCode(const QString& code);

private:
	static mScriptTextBuffer* createTextBuffer(void* context);

	struct Logger : mLogger {
		ScriptingController* p;
	} m_logger{};

	mScriptContext m_scriptContext;

	mScriptEngineContext* m_activeEngine = nullptr;
	QHash<QString, mScriptEngineContext*> m_engines;

	std::shared_ptr<CoreController> m_controller;
};

}
