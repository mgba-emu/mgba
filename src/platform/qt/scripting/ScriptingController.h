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

#include "scripting/AutorunScriptModel.h"
#include "VFileDevice.h"

#include <memory>

class QKeyEvent;
class QTextDocument;

struct VideoBackend;

namespace QGBA {

class ConfigController;
class CoreController;
class InputController;
class ScriptingTextBuffer;
class ScriptingTextBufferModel;

class ScriptingController : public QObject {
Q_OBJECT

public:
	ScriptingController(ConfigController* config, QObject* parent = nullptr);
	~ScriptingController();

	void setController(std::shared_ptr<CoreController> controller);
	void setInputController(InputController* controller);
	void setVideoBackend(VideoBackend* backend);

	bool loadFile(const QString& path);
	bool load(VFileDevice& vf, const QString& name);

	void scriptingEvent(QObject* obj, QEvent* ev);

	mScriptContext* context() { return &m_scriptContext; }
	ScriptingTextBufferModel* textBufferModel() const { return m_bufferModel; }

	QString getFilenameFilters() const;

signals:
	void log(const QString&);
	void warn(const QString&);
	void error(const QString&);
	void textBufferCreated(ScriptingTextBuffer*);

	void autorunScriptsOpened(QWidget* view);

public slots:
	void clearController();
	void updateVideoScale();
	void reset();
	void runCode(const QString& code);
	void openAutorunEdit();

	void flushStorage();

protected:
	bool eventFilter(QObject*, QEvent*) override;

private slots:
	void updateGamepad();
	void attach();
	void saveAutorun(const QList<QVariant>& autorun);

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
	VideoBackend* m_videoBackend = nullptr;

	mScriptGamepad m_gamepad;

	AutorunScriptModel m_model;
	std::shared_ptr<CoreController> m_controller;
	InputController* m_inputController = nullptr;
	ConfigController* m_config = nullptr;

	QTimer m_storageFlush;
};

}
