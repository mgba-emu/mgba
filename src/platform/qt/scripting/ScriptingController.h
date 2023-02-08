/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QHash>
#include <QObject>
#include <QTimer>

#include <mgba/script/context.h>
#include <mgba/script/input.h>
#include <mgba/core/scripting.h>

#include "VFileDevice.h"

#include <memory>

class QKeyEvent;
class QTextDocument;

namespace QGBA {

class CoreController;
class InputController;
class ScriptingTextBuffer;
class ScriptingTextBufferModel;

class ScriptingController : public QObject {
Q_OBJECT

public:
	ScriptingController(QObject* parent = nullptr);
	~ScriptingController();

	void setController(std::shared_ptr<CoreController> controller);
	void setInputController(InputController* controller);

	bool loadFile(const QString& path);
	bool load(VFileDevice& vf, const QString& name);

	void event(QObject* obj, QEvent* ev);

	mScriptContext* context() { return &m_scriptContext; }
	ScriptingTextBufferModel* textBufferModel() const { return m_bufferModel; }

signals:
	void log(const QString&);
	void warn(const QString&);
	void error(const QString&);
	void textBufferCreated(ScriptingTextBuffer*);

public slots:
	void clearController();
	void reset();
	void runCode(const QString& code);

	void flushStorage();

protected:
	bool eventFilter(QObject*, QEvent*) override;

private slots:
	void updateGamepad();

private:
	void init();

	void attachGamepad();
	void detachGamepad();

	static uint32_t qtToScriptingKey(const QKeyEvent*);
	static uint16_t qtToScriptingModifiers(Qt::KeyboardModifiers);

	struct Logger : mLogger {
		ScriptingController* p;
	} m_logger{};

	mScriptContext m_scriptContext;

	mScriptEngineContext* m_activeEngine = nullptr;
	QHash<QString, mScriptEngineContext*> m_engines;
	ScriptingTextBufferModel* m_bufferModel;

	mScriptGamepad m_gamepad;

	std::shared_ptr<CoreController> m_controller;
	InputController* m_inputController = nullptr;

	QTimer m_storageFlush;
};

}
