/* Copyright (c) 2013-2025 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "scripting/AutorunScriptView.h"

#include "GBAApp.h"
#include "scripting/AutorunScriptModel.h"
#include "scripting/ScriptingController.h"

using namespace QGBA;

AutorunScriptView::AutorunScriptView(AutorunScriptModel* model, ScriptingController* controller, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
	, m_controller(controller)
{
	m_ui.setupUi(this);

	m_ui.autorunList->setModel(model);
	m_ui.autorunList->setDragEnabled(true);
	m_ui.autorunList->viewport()->setAcceptDrops(true);
	m_ui.autorunList->setDropIndicatorShown(true);
	m_ui.autorunList->setDragDropMode(QAbstractItemView::InternalMove);
}

void AutorunScriptView::addScript() {
	QString filename = GBAApp::app()->getOpenFileName(this, tr("Select a script"), m_controller->getFilenameFilters());
	if (filename.isEmpty()) {
		return;
	}

	AutorunScriptModel* model = static_cast<AutorunScriptModel*>(m_ui.autorunList->model());
	model->addScript(filename);
}

void AutorunScriptView::removeScript(const QModelIndex& index) {
	QAbstractItemModel* model = m_ui.autorunList->model();
	model->removeRow(index.row(), index.parent());
}

void AutorunScriptView::removeScript() {
	removeScript(m_ui.autorunList->currentIndex());
}

void AutorunScriptView::moveUp() {
	QModelIndex index = m_ui.autorunList->currentIndex();
	QAbstractItemModel* model = m_ui.autorunList->model();
	model->moveRows(index.parent(), index.row(), 1, index.parent(), index.row() - 1);
}

void AutorunScriptView::moveDown() {
	QModelIndex index = m_ui.autorunList->currentIndex();
	QAbstractItemModel* model = m_ui.autorunList->model();
	model->moveRows(index.parent(), index.row(), 1, index.parent(), index.row() + 2);
}
