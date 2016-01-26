/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "CheatsView.h"

#include "GBAApp.h"
#include "GameController.h"

#include <QClipboard>

extern "C" {
#include "gba/cheats.h"
}

using namespace QGBA;

CheatsView::CheatsView(GameController* controller, QWidget* parent)
	: QWidget(parent)
	, m_controller(controller)
	, m_model(controller->cheatDevice())
{
	m_ui.setupUi(this);

	m_ui.cheatList->installEventFilter(this);
	m_ui.cheatList->setModel(&m_model);

	connect(m_ui.load, SIGNAL(clicked()), this, SLOT(load()));
	connect(m_ui.save, SIGNAL(clicked()), this, SLOT(save()));
	connect(m_ui.addSet, SIGNAL(clicked()), this, SLOT(addSet()));
	connect(m_ui.remove, SIGNAL(clicked()), this, SLOT(removeSet()));
	connect(controller, SIGNAL(gameStopped(GBAThread*)), &m_model, SLOT(invalidated()));
	connect(controller, SIGNAL(stateLoaded(GBAThread*)), &m_model, SLOT(invalidated()));

	connect(m_ui.add, &QPushButton::clicked, [this]() {
		enterCheat(GBACheatAddLine);
	});

	connect(m_ui.addGSA, &QPushButton::clicked, [this]() {
		enterCheat(GBACheatAddGameSharkLine);
	});

	connect(m_ui.addPAR, &QPushButton::clicked, [this]() {
		enterCheat(GBACheatAddProActionReplayLine);
	});

	connect(m_ui.addCB, &QPushButton::clicked, [this]() {
		enterCheat(GBACheatAddCodeBreakerLine);
	});
}

bool CheatsView::eventFilter(QObject* object, QEvent* event) {
	if (object != m_ui.cheatList) {
		return false;
	}
	if (event->type() != QEvent::KeyPress) {
		return false;
	}
	if (static_cast<QKeyEvent*>(event) == QKeySequence::Copy) {
		QApplication::clipboard()->setText(m_model.toString(m_ui.cheatList->selectionModel()->selectedIndexes()));
		return true;
	}
	return false;
}

void CheatsView::load() {
	QString filename = GBAApp::app()->getOpenFileName(this, tr("Select cheats file"));
	if (!filename.isEmpty()) {
		m_model.loadFile(filename);
	}
}

void CheatsView::save() {
	QString filename = GBAApp::app()->getSaveFileName(this, tr("Select cheats file"));
	if (!filename.isEmpty()) {
		m_model.saveFile(filename);
	}
}

void CheatsView::addSet() {
	GBACheatSet* set = new GBACheatSet;
	GBACheatSetInit(set, nullptr);
	m_controller->threadInterrupt();
	m_model.addSet(set);
	m_controller->threadContinue();
}

void CheatsView::removeSet() {
	GBACheatSet* set;
	QModelIndexList selection = m_ui.cheatList->selectionModel()->selectedIndexes();
	if (selection.count() < 1) {
		return;
	}
	m_controller->threadInterrupt();
	for (const QModelIndex& index : selection) {
		m_model.removeAt(selection[0]);
	}
	m_controller->threadContinue();
}

void CheatsView::enterCheat(std::function<bool(GBACheatSet*, const char*)> callback) {
	GBACheatSet* set = nullptr;
	QModelIndexList selection = m_ui.cheatList->selectionModel()->selectedIndexes();
	QModelIndex index;
	if (selection.count() == 0) {
		set = new GBACheatSet;
		GBACheatSetInit(set, nullptr);
	} else if (selection.count() == 1) {
		index = selection[0];
		set = m_model.itemAt(index);
	}

	if (!set) {
		return;
	}
	m_controller->threadInterrupt();
	if (selection.count() == 0) {
		m_model.addSet(set);
		index = m_model.index(m_model.rowCount() - 1, 0, QModelIndex());
		m_ui.cheatList->selectionModel()->select(index, QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
	}
	QStringList cheats = m_ui.codeEntry->toPlainText().split('\n', QString::SkipEmptyParts);
	for (const QString& string : cheats) {
		m_model.beginAppendRow(index);
		callback(set, string.toUtf8().constData());
		m_model.endAppendRow();
	}
	m_controller->threadContinue();
	m_ui.codeEntry->clear();
}
