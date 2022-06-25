/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "scripting/ScriptingView.h"

#include "GBAApp.h"
#include "ConfigController.h"
#include "scripting/ScriptingController.h"
#include "scripting/ScriptingTextBuffer.h"
#include "scripting/ScriptingTextBufferModel.h"

using namespace QGBA;

ScriptingView::ScriptingView(ScriptingController* controller, ConfigController* config, QWidget* parent)
	: QMainWindow(parent)
	, m_config(config)
	, m_controller(controller)
{
	m_ui.setupUi(this);

	ScriptingTextBufferModel* bufferModel = controller->textBufferModel();
	m_ui.prompt->setFont(GBAApp::app()->monospaceFont());
	m_ui.log->setNewlineTerminated(true);
	m_ui.buffers->setModel(bufferModel);

	connect(m_ui.prompt, &QLineEdit::returnPressed, this, &ScriptingView::submitRepl);
	connect(m_ui.runButton, &QAbstractButton::clicked, this, &ScriptingView::submitRepl);

	connect(bufferModel, &QAbstractItemModel::modelAboutToBeReset, this, &ScriptingView::controllerReset);
	connect(bufferModel, &QAbstractItemModel::rowsInserted, this, [this, bufferModel](const QModelIndex&, int row, int) {
		m_ui.buffers->setCurrentIndex(bufferModel->index(row, 0));
	});

	connect(m_controller, &ScriptingController::log, m_ui.log, &LogWidget::log);
	connect(m_controller, &ScriptingController::warn, m_ui.log, &LogWidget::warn);
	connect(m_controller, &ScriptingController::error, m_ui.log, &LogWidget::error);

	connect(m_ui.buffers->selectionModel(), &QItemSelectionModel::currentChanged, this, &ScriptingView::selectBuffer);
	connect(m_ui.load, &QAction::triggered, this, &ScriptingView::load);
	connect(m_ui.reset, &QAction::triggered, controller, &ScriptingController::reset);

	m_mruFiles = m_config->getMRU(ConfigController::MRU::Script);
	updateMRU();

	m_blankDocument = new QTextDocument(this);
	m_blankDocument->setDocumentLayout(new QPlainTextDocumentLayout(m_blankDocument));

	m_ui.buffers->setCurrentIndex(bufferModel->index(0, 0));
}

void ScriptingView::submitRepl() {
	m_ui.log->echo(m_ui.prompt->text());
	m_controller->runCode(m_ui.prompt->text());
	m_ui.prompt->clear();
}

void ScriptingView::load() {
	QString filename = GBAApp::app()->getOpenFileName(this, tr("Select script to load"), getFilters());
	if (!filename.isEmpty()) {
		if (!m_controller->loadFile(filename)) {
			return;
		}
		appendMRU(filename);
	}
}

void ScriptingView::controllerReset() {
	selectBuffer(QModelIndex());
}

void ScriptingView::selectBuffer(const QModelIndex& current, const QModelIndex&) {
	if (current.isValid()) {
		m_ui.buffer->setDocument(current.data(ScriptingTextBufferModel::DocumentRole).value<QTextDocument*>());
	} else {
		// If there is no selected buffer, use the blank document.
		m_ui.buffer->setDocument(m_blankDocument);
	}
}

QString ScriptingView::getFilters() const {
	QStringList filters;
#ifdef USE_LUA
	filters.append(tr("Lua scripts (*.lua)"));
#endif
	filters.append(tr("All files (*.*)"));
	return filters.join(";;");
}

void ScriptingView::appendMRU(const QString& fname) {
	int index = m_mruFiles.indexOf(fname);
	if (index >= 0) {
		m_mruFiles.removeAt(index);
	}
	m_mruFiles.prepend(fname);
	while (m_mruFiles.size() > ConfigController::MRU_LIST_SIZE) {
		m_mruFiles.removeLast();
	}
	updateMRU();
}

void ScriptingView::updateMRU() {
	m_config->setMRU(m_mruFiles, ConfigController::MRU::Script);
	m_ui.mru->clear();
	for (const auto& fname : m_mruFiles) {
		m_ui.mru->addAction(fname, [this, fname]() {
			m_controller->loadFile(fname);
		});
	}
}
