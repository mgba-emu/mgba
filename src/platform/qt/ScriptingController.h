/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QHash>
#include <QAbstractListModel>

#include <mgba/script/context.h>
#include <mgba/core/scripting.h>

#include "VFileDevice.h"

#include <memory>

class QTextDocument;

namespace QGBA {

class CoreController;
class ScriptingTextBuffer;

class ScriptingController : public QAbstractListModel {
Q_OBJECT

public:
	enum ItemDataRole {
		DocumentRole = Qt::UserRole + 1,
	};

	ScriptingController(QObject* parent = nullptr);
	~ScriptingController();

	void setController(std::shared_ptr<CoreController> controller);

	bool loadFile(const QString& path);
	bool load(VFileDevice& vf, const QString& name);

	mScriptContext* context() { return &m_scriptContext; }
	QList<ScriptingTextBuffer*> textBuffers() { return m_buffers; }

	int rowCount(const QModelIndex& parent = QModelIndex()) const;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;

signals:
	void log(const QString&);
	void warn(const QString&);
	void error(const QString&);
	void textBufferCreated(ScriptingTextBuffer*);

public slots:
	void clearController();
	void reset();
	void runCode(const QString& code);

private slots:
	void bufferNameChanged(const QString&);

private:
	void init();

	static mScriptTextBuffer* createTextBuffer(void* context);

	struct Logger : mLogger {
		ScriptingController* p;
	} m_logger{};

	mScriptContext m_scriptContext;

	mScriptEngineContext* m_activeEngine = nullptr;
	QHash<QString, mScriptEngineContext*> m_engines;
	QList<ScriptingTextBuffer*> m_buffers;

	std::shared_ptr<CoreController> m_controller;
};

}
