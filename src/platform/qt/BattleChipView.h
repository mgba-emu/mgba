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
class Window;

class BattleChipView : public QDialog {
Q_OBJECT

public:
	BattleChipView(std::shared_ptr<CoreController> controller, Window* window, QWidget* parent = nullptr);
	~BattleChipView();

public slots:
	void setFlavor(int);
	void insertChip(bool);
	void reinsert();

private slots:
	void advanceFrameCounter();
	void addChip();
	void addChipId(int);
	void removeChip();

	void saveDeck();
	void loadDeck();

private:
	static const int UNINSERTED_TIME = 10;

	void loadChipNames(int);

	Ui::BattleChipView m_ui;

	QMap<int, int> m_chipIndexToId;
	QMap<int, QString> m_chipIdToName;
	std::shared_ptr<CoreController> m_controller;
	int m_flavor;

	int m_frameCounter = -1;
	bool m_next = false;

	Window* m_window;
};

}