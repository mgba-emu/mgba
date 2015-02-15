/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "CheatsView.h"

#include "GameController.h"

#include <QFileDialog>

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

	m_ui.cheatList->setModel(&m_model);

	connect(m_ui.load, SIGNAL(clicked()), this, SLOT(load()));
	connect(m_ui.addSet, SIGNAL(clicked()), this, SLOT(addSet()));
	connect(m_ui.remove, SIGNAL(clicked()), this, SLOT(removeSet()));
	connect(controller, SIGNAL(gameStopped(GBAThread*)), &m_model, SLOT(invalidated()));

	connect(m_ui.add, &QPushButton::clicked, [this]() {
		enterCheat(GBACheatAddLine);
	});

	connect(m_ui.addGSA, &QPushButton::clicked, [this]() {
		enterCheat(GBACheatAddGameSharkLine);
	});

	connect(m_ui.addCB, &QPushButton::clicked, [this]() {
		enterCheat(GBACheatAddCodeBreakerLine);
	});
}

void CheatsView::load() {
	QString filename = QFileDialog::getOpenFileName(this, tr("Select cheats file"));
	if (!filename.isEmpty()) {
		m_model.loadFile(filename);
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
	if (selection.count() != 1) {
		return;
	}
	m_controller->threadInterrupt();
	m_model.removeAt(selection[0]);
	m_controller->threadContinue();
}

void CheatsView::enterCheat(std::function<bool(GBACheatSet*, const char*)> callback) {
	GBACheatSet* set;
	QModelIndexList selection = m_ui.cheatList->selectionModel()->selectedIndexes();
	if (selection.count() != 1) {
		return;
	}
	set = m_model.itemAt(selection[0]);
	if (!set) {
		return;
	}
	m_controller->threadInterrupt();
	QStringList cheats = m_ui.codeEntry->toPlainText().split('\n', QString::SkipEmptyParts);
	for (const QString& string : cheats) {
		callback(set, string.toLocal8Bit().constData());
	}
	m_controller->threadContinue();
	m_ui.codeEntry->clear();
}