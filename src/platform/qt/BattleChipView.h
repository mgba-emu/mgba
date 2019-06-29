/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QDialog>

#include <memory>

#include <mgba/core/interface.h>

#include "ui_BattleChipView.h"

namespace QGBA {

class CoreController;

class BattleChipView : public QDialog {
Q_OBJECT

public:
	BattleChipView(std::shared_ptr<CoreController> controller, QWidget* parent = nullptr);
	~BattleChipView();

public slots:
	void insertChip(bool);

private:
	Ui::BattleChipView m_ui;

	std::shared_ptr<CoreController> m_controller;
};

}