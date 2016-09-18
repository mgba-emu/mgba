/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_CHEATS_VIEW
#define QGBA_CHEATS_VIEW

#include <QWidget>

#include <functional>

#include "CheatsModel.h"

#include "ui_CheatsView.h"

struct mCheatDevice;

namespace QGBA {

class GameController;

class CheatsView : public QWidget {
Q_OBJECT

public:
	CheatsView(GameController* controller, QWidget* parent = nullptr);

	virtual bool eventFilter(QObject*, QEvent*) override;

private slots:
	void load();
	void save();
	void addSet();
	void removeSet();

private:
	void enterCheat(int codeType);

	Ui::CheatsView m_ui;
	GameController* m_controller;
	CheatsModel m_model;
};

}

#endif
