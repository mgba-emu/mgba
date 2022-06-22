/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ScriptingController.h"

#include "CoreController.h"
#include "ScriptingTextBuffer.h"

using namespace QGBA;

ScriptingController::ScriptingController(QObject* parent)
	: QAbstractListModel(parent)
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
	return load(vf, path);
}

bool ScriptingController::load(VFileDevice& vf, const QString& name) {
	if (!m_activeEngine) {
		return false;
	}
	QByteArray utf8 = name.toUtf8();
	CoreController::Interrupter interrupter(m_controller);
	if (!m_activeEngine->load(m_activeEngine, utf8.constData(), vf) || !m_activeEngine->run(m_activeEngine)) {
		emit error(QString::fromUtf8(m_activeEngine->getError(m_activeEngine)));
		return false;
	}
	return true;
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
	beginResetModel();
	QList<ScriptingTextBuffer*> toDelete = m_buffers;
	m_buffers.clear();
	endResetModel();
	CoreController::Interrupter interrupter(m_controller);
	for (ScriptingTextBuffer* buffer : toDelete) {
		delete buffer;
	}
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

mScriptTextBuffer* ScriptingController::createTextBuffer(void* context) {
	ScriptingController* self = static_cast<ScriptingController*>(context);
	self->beginInsertRows(QModelIndex(), self->m_buffers.size(), self->m_buffers.size() + 1);
	ScriptingTextBuffer* buffer = new ScriptingTextBuffer(self);
	QObject::connect(buffer, &ScriptingTextBuffer::bufferNameChanged, self, &ScriptingController::bufferNameChanged);
	self->m_buffers.append(buffer);
	emit self->textBufferCreated(buffer);
	self->endInsertRows();
	return buffer->textBuffer();
}

void ScriptingController::init() {
	mScriptContextInit(&m_scriptContext);
	mScriptContextAttachStdlib(&m_scriptContext);
	mScriptContextRegisterEngines(&m_scriptContext);

	mScriptContextAttachLogger(&m_scriptContext, &m_logger);
	mScriptContextSetTextBufferFactory(&m_scriptContext, &ScriptingController::createTextBuffer, this);

	HashTableEnumerate(&m_scriptContext.engines, [](const char* key, void* engine, void* context) {
	ScriptingController* self = static_cast<ScriptingController*>(context);
		self->m_engines[QString::fromUtf8(key)] = static_cast<mScriptEngineContext*>(engine);
	}, this);

	if (m_engines.count() == 1) {
		m_activeEngine = *m_engines.begin();
	}
}

void ScriptingController::bufferNameChanged(const QString&) {
	ScriptingTextBuffer* buffer = qobject_cast<ScriptingTextBuffer*>(sender());
	int row = m_buffers.indexOf(buffer);
	if (row < 0) {
		return;
	}
	QModelIndex idx = index(row, 0);
	emit dataChanged(idx, idx, { Qt::DisplayRole });
}

int ScriptingController::rowCount(const QModelIndex& parent) const {
	if (parent.isValid()) {
		return 0;
	}
	return m_buffers.size();
}

QVariant ScriptingController::data(const QModelIndex& index, int role) const {
	if (index.parent().isValid() || index.row() < 0 || index.row() >= m_buffers.size() || index.column() != 0) {
		return QVariant();
	}
	if (role == Qt::DisplayRole) {
		return m_buffers[index.row()]->document()->metaInformation(QTextDocument::DocumentTitle);
	} else if (role == ScriptingController::DocumentRole) {
		return QVariant::fromValue<QTextDocument*>(m_buffers[index.row()]->document());
	}
	return QVariant();
}
