/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_LOAD_SAVE_STATE
#define QGBA_LOAD_SAVE_STATE

#include <QWidget>

#include "ui_LoadSaveState.h"

namespace QGBA {

class GameController;
class InputController;
class SavestateButton;

enum class LoadSave {
	LOAD,
	SAVE
};

class LoadSaveState : public QWidget {
Q_OBJECT

public:
	const static int NUM_SLOTS = 9;

	LoadSaveState(GameController* controller, QWidget* parent = nullptr);

	void setInputController(InputController* controller);
	void setMode(LoadSave mode);

signals:
	void closed();

protected:
	virtual bool eventFilter(QObject*, QEvent*) override;
	virtual void closeEvent(QCloseEvent*) override;
	virtual void showEvent(QShowEvent*) override;
	virtual void paintEvent(QPaintEvent*) override;

private:
	void loadState(int slot);
	void triggerState(int slot);

	Ui::LoadSaveState m_ui;
	GameController* m_controller;
	SavestateButton* m_slots[NUM_SLOTS];
	LoadSave m_mode;

	int m_currentFocus;
	QPixmap m_currentImage;
};

}

#endif
