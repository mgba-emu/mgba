/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "scripting/ScriptingController.h"

#include "AudioProcessor.h"
#include "CoreController.h"
#include "scripting/ScriptingTextBuffer.h"
#include "scripting/ScriptingTextBufferModel.h"

using namespace QGBA;

ScriptingController::ScriptingController(QObject* parent)
	: QObject(parent)
{
	m_logger.p = this;
	m_logger.log = [](mLogger* log, int, enum mLogLevel level, const char* format, va_list args) {
		Logger* logger = static_cast<Logger*>(log);
		va_list argc;
		va_copy(argc, args);
		QString message = QString::vasprintf(format, argc);
		va_end(argc);
		switch (level) {
		case mLOG_WARN:
			emit logger->p->warn(message);
			break;
		case mLOG_ERROR:
			emit logger->p->error(message);
			break;
		default:
			emit logger->p->log(message);
			break;
		}
	};

	m_bufferModel = new ScriptingTextBufferModel(this);
	QObject::connect(m_bufferModel, &ScriptingTextBufferModel::textBufferCreated, this, &ScriptingController::textBufferCreated);

	init();
}

ScriptingController::~ScriptingController() {
	clearController();
	mScriptContextDeinit(&m_scriptContext);
}

void ScriptingController::setController(std::shared_ptr<CoreController> controller) {
	if (controller == m_controller) {
		return;
	}
	clearController();
	m_controller = controller;
	CoreController::Interrupter interrupter(m_controller);
	m_controller->thread()->scriptContext = &m_scriptContext;
	if (m_controller->hasStarted()) {
		mScriptContextAttachCore(&m_scriptContext, m_controller->thread()->core);
	}
	connect(m_controller.get(), &CoreController::stopping, this, &ScriptingController::clearController);
}

bool ScriptingController::loadFile(const QString& path) {
	VFileDevice vf(path, QIODevice::ReadOnly);
	if (!vf.isOpen()) {
		return false;
	}
	return load(vf, path);
}

bool ScriptingController::load(VFileDevice& vf, const QString& name) {
	if (!m_activeEngine) {
		return false;
	}
	QByteArray utf8 = name.toUtf8();
	CoreController::Interrupter interrupter(m_controller);
	if (m_controller) {
		m_controller->setSync(false);
		m_controller->unpaused();
	}
	bool ok = true;
	if (!m_activeEngine->load(m_activeEngine, utf8.constData(), vf) || !m_activeEngine->run(m_activeEngine)) {
		ok = false;
	}
	if (m_controller) {
		m_controller->setSync(true);
		if (m_controller->isPaused()) {
			m_controller->paused();
		}
	}
	return ok;
}

void ScriptingController::clearController() {
	if (!m_controller) {
		return;
	}
	{
		CoreController::Interrupter interrupter(m_controller);
		mScriptContextDetachCore(&m_scriptContext);
		m_controller->thread()->scriptContext = nullptr;
	}
	m_controller.reset();
}

void ScriptingController::reset() {
	CoreController::Interrupter interrupter(m_controller);
	m_bufferModel->reset();
	mScriptContextDetachCore(&m_scriptContext);
	mScriptContextDeinit(&m_scriptContext);
	m_engines.clear();
	m_activeEngine = nullptr;
	init();
	if (m_controller && m_controller->hasStarted()) {
		mScriptContextAttachCore(&m_scriptContext, m_controller->thread()->core);
	}
}

void ScriptingController::runCode(const QString& code) {
	VFileDevice vf(code.toUtf8());
	load(vf, "*prompt");
}

void ScriptingController::init() {
	mScriptContextInit(&m_scriptContext);
	mScriptContextAttachStdlib(&m_scriptContext);
	mScriptContextAttachSocket(&m_scriptContext);
	mScriptContextRegisterEngines(&m_scriptContext);

	mScriptContextAttachLogger(&m_scriptContext, &m_logger);
	m_bufferModel->attachToContext(&m_scriptContext);

	HashTableEnumerate(&m_scriptContext.engines, [](const char* key, void* engine, void* context) {
	ScriptingController* self = static_cast<ScriptingController*>(context);
		self->m_engines[QString::fromUtf8(key)] = static_cast<mScriptEngineContext*>(engine);
	}, this);

	if (m_engines.count() == 1) {
		m_activeEngine = *m_engines.begin();
	}
}
