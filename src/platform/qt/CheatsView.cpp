/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "CheatsView.h"

#include "GBAApp.h"
#include "CoreController.h"
#include "LogController.h"

#include <QBoxLayout>
#include <QButtonGroup>
#include <QClipboard>
#include <QRadioButton>
#include <QRegularExpression>

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
	m_ui.codeEntry->setFont(GBAApp::app()->monospaceFont());

	connect(m_ui.load, &QAbstractButton::clicked, this, &CheatsView::load);
	connect(m_ui.save, &QAbstractButton::clicked, this, &CheatsView::save);
	connect(m_ui.addSet, &QAbstractButton::clicked, this, &CheatsView::addSet);
	connect(m_ui.remove, &QAbstractButton::clicked, this, &CheatsView::removeSet);
	connect(m_ui.add, &QAbstractButton::clicked, this, &CheatsView::enterCheat);
	connect(controller.get(), &CoreController::stopping, this, &CheatsView::close);
	connect(controller.get(), &CoreController::stateLoaded, &m_model, &CheatsModel::invalidated);

	switch (controller->platform()) {
#ifdef M_CORE_GBA
	case mPLATFORM_GBA:
		registerCodeType(tr("Autodetect (recommended)"), GBA_CHEAT_AUTODETECT);
		registerCodeType(QLatin1String("GameShark"), GBA_CHEAT_GAMESHARK);
		registerCodeType(QLatin1String("Action Replay MAX"), GBA_CHEAT_PRO_ACTION_REPLAY);
		registerCodeType(QLatin1String("CodeBreaker"), GBA_CHEAT_CODEBREAKER);
		break;
#endif
#ifdef M_CORE_GB
	case mPLATFORM_GB:
		registerCodeType(tr("Autodetect (recommended)"), GB_CHEAT_AUTODETECT);
		registerCodeType(QLatin1String("GameShark"), GB_CHEAT_GAMESHARK);
		registerCodeType(QLatin1String("Game Genie"), GB_CHEAT_GAME_GENIE);
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
	QString filename = GBAApp::app()->getOpenFileName(this, tr("Select cheats file"), tr(("Cheats file (*.cheats *.cht)")));
	if (!filename.isEmpty()) {
		m_model.loadFile(filename);
	}
}

void CheatsView::save() {
	QString filename = GBAApp::app()->getSaveFileName(this, tr("Select cheats file"), tr(("Cheats file (*.cheats)")));
	if (!filename.isEmpty()) {
		m_model.saveFile(filename);
	}
}

void CheatsView::addSet() {
	CoreController::Interrupter interrupter(m_controller);
	mCheatSet* set = m_controller->cheatDevice()->createSet(m_controller->cheatDevice(), nullptr);
	m_model.addSet(set);
	m_ui.cheatList->selectionModel()->select(m_model.index(m_model.rowCount() - 1, 0, QModelIndex()), QItemSelectionModel::ClearAndSelect);
	enterCheat();
}

void CheatsView::removeSet() {
	QModelIndexList selection = m_ui.cheatList->selectionModel()->selectedIndexes();
	if (selection.count() < 1) {
		return;
	}
	CoreController::Interrupter interrupter(m_controller);
	for (const QModelIndex& index ATTRIBUTE_UNUSED : selection) {
		m_model.removeAt(selection[0]);
	}
}

void CheatsView::registerCodeType(const QString& label, int type) {
	QRadioButton* add = new QRadioButton(label);
	m_ui.typeLayout->addWidget(add);
	connect(add, &QAbstractButton::clicked, [this, type]() {
		m_codeType = type;
	});
	if (!m_typeGroup) {
		m_typeGroup = new QButtonGroup(this);
		m_codeType = type;
		add->setChecked(true);
	}
	m_typeGroup->addButton(add);
}

void CheatsView::enterCheat() {
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
	// TODO: Update API to handle this splitting in the core
	QRegularExpression regexp("\\s");
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
	QStringList cheats = m_ui.codeEntry->toPlainText().split(regexp, Qt::SkipEmptyParts);
#else
	QStringList cheats = m_ui.codeEntry->toPlainText().split(regexp, QString::SkipEmptyParts);
#endif
	int failure = 0;
	QString buffer;
	for (const QString& string : cheats) {
		m_model.beginAppendRow(index);
		if (!buffer.isEmpty()) {
			buffer += " " + string;
			if (mCheatAddLine(set, buffer.toUtf8().constData(), m_codeType)) {
				buffer.clear();
			} else if (mCheatAddLine(set, string.toUtf8().constData(), m_codeType)) {
				buffer.clear();
			} else {
				buffer = string;
				++failure;
			}
		} else if (!mCheatAddLine(set, string.toUtf8().constData(), m_codeType)) {
			buffer = string;
		}
		m_model.endAppendRow();
	}
	if (!buffer.isEmpty()) {
		++failure;
	}
	if (set->refresh) {
		set->refresh(set, m_controller->cheatDevice());
	}
	if (failure) {
		LOG(QT, ERROR) << tr("Some cheats could not be added. Please ensure they're formatted correctly and/or try other cheat types.");
	}
	m_ui.codeEntry->clear();
}
