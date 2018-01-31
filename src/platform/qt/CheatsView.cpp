/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "CheatsView.h"

#include "GBAApp.h"
#include "CoreController.h"

#include <QClipboard>
#include <QPushButton>

#include <mgba/core/cheats.h>
#ifdef M_CORE_GBA
#include <mgba/internal/gba/cheats.h>
#endif
#ifdef M_CORE_GB
#include <mgba/internal/gb/cheats.h>
#endif

using namespace QGBA;

CheatsView::CheatsView(std::shared_ptr<CoreController> controller, QWidget* parent)
	: QWidget(parent)
	, m_controller(controller)
	, m_model(controller->cheatDevice())
{
	m_ui.setupUi(this);

	m_ui.cheatList->installEventFilter(this);
	m_ui.cheatList->setModel(&m_model);

	connect(m_ui.load, &QPushButton::clicked, this, &CheatsView::load);
	connect(m_ui.save, &QPushButton::clicked, this, &CheatsView::save);
	connect(m_ui.addSet, &QPushButton::clicked, this, &CheatsView::addSet);
	connect(m_ui.remove, &QPushButton::clicked, this, &CheatsView::removeSet);
	connect(controller.get(), &CoreController::stopping, this, &CheatsView::close);
	connect(controller.get(), &CoreController::stateLoaded, &m_model, &CheatsModel::invalidated);

	QPushButton* add;
	switch (controller->platform()) {
#ifdef M_CORE_GBA
	case PLATFORM_GBA:
		connect(m_ui.add, &QPushButton::clicked, [this]() {
			enterCheat(GBA_CHEAT_AUTODETECT);
		});

		add = new QPushButton(tr("Add GameShark"));
		m_ui.gridLayout->addWidget(add, m_ui.gridLayout->rowCount(), 2, 1, 2);
		connect(add, &QPushButton::clicked, [this]() {
			enterCheat(GBA_CHEAT_GAMESHARK);
		});

		add = new QPushButton(tr("Add Pro Action Replay"));
		m_ui.gridLayout->addWidget(add, m_ui.gridLayout->rowCount(), 2, 1, 2);
		connect(add, &QPushButton::clicked, [this]() {
			enterCheat(GBA_CHEAT_PRO_ACTION_REPLAY);
		});

		add = new QPushButton(tr("Add CodeBreaker"));
		m_ui.gridLayout->addWidget(add, m_ui.gridLayout->rowCount(), 2, 1, 2);
		connect(add, &QPushButton::clicked, [this]() {
			enterCheat(GBA_CHEAT_CODEBREAKER);
		});
		break;
#endif
#ifdef M_CORE_GB
	case PLATFORM_GB:
		connect(m_ui.add, &QPushButton::clicked, [this]() {
			enterCheat(GB_CHEAT_AUTODETECT);
		});

		add = new QPushButton(tr("Add GameShark"));
		m_ui.gridLayout->addWidget(add, m_ui.gridLayout->rowCount(), 2, 1, 2);
		connect(add, &QPushButton::clicked, [this]() {
			enterCheat(GB_CHEAT_GAMESHARK);
		});

		add = new QPushButton(tr("Add GameGenie"));
		m_ui.gridLayout->addWidget(add, m_ui.gridLayout->rowCount(), 2, 1, 2);
		connect(add, &QPushButton::clicked, [this]() {
			enterCheat(GB_CHEAT_GAME_GENIE);
		});
		break;
#endif
	default:
		break;
	}

	// Stretch the cheat list back into place
	int index = m_ui.gridLayout->indexOf(m_ui.cheatList);
	m_ui.gridLayout->takeAt(index);
	m_ui.gridLayout->addWidget(m_ui.cheatList, 0, 0, -1, 1);
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
	QString filename = GBAApp::app()->getOpenFileName(this, tr("Select cheats file"), tr(("Cheats file (*.cheats *.cht *.clt)")));
	if (!filename.isEmpty()) {
		m_model.loadFile(filename);
	}
}

void CheatsView::save() {
	QString filename = GBAApp::app()->getSaveFileName(this, tr("Select cheats file"), tr(("Cheats file (*.cheats *.cht *.clt)")));
	if (!filename.isEmpty()) {
		m_model.saveFile(filename);
	}
}

void CheatsView::addSet() {
	CoreController::Interrupter interrupter(m_controller);
	mCheatSet* set = m_controller->cheatDevice()->createSet(m_controller->cheatDevice(), nullptr);
	m_model.addSet(set);
}

void CheatsView::removeSet() {
	GBACheatSet* set;
	QModelIndexList selection = m_ui.cheatList->selectionModel()->selectedIndexes();
	if (selection.count() < 1) {
		return;
	}
	CoreController::Interrupter interrupter(m_controller);
	for (const QModelIndex& index : selection) {
		m_model.removeAt(selection[0]);
	}
}

void CheatsView::enterCheat(int codeType) {
	mCheatSet* set = nullptr;
	QModelIndexList selection = m_ui.cheatList->selectionModel()->selectedIndexes();
	QModelIndex index;
	if (selection.count() == 0) {
		set = m_controller->cheatDevice()->createSet(m_controller->cheatDevice(), nullptr);
	} else if (selection.count() == 1) {
		index = selection[0];
		set = m_model.itemAt(index);
	}

	if (!set) {
		return;
	}
	CoreController::Interrupter interrupter(m_controller);
	if (selection.count() == 0) {
		m_model.addSet(set);
		index = m_model.index(m_model.rowCount() - 1, 0, QModelIndex());
		m_ui.cheatList->selectionModel()->select(index, QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
	}
	QStringList cheats = m_ui.codeEntry->toPlainText().split('\n', QString::SkipEmptyParts);
	for (const QString& string : cheats) {
		m_model.beginAppendRow(index);
		mCheatAddLine(set, string.toUtf8().constData(), codeType);
		m_model.endAppendRow();
	}
	set->refresh(set, m_controller->cheatDevice());
	m_ui.codeEntry->clear();
}
