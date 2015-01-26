/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_GAMEPAK_VIEW
#define QGBA_GAMEPAK_VIEW

#include <QWidget>

#include "ui_GamePakView.h"

extern "C" {
#include "gba-overrides.h"
}

struct GBAThread;

namespace QGBA {

class ConfigController;
class GameController;

class GamePakView : public QWidget {
Q_OBJECT

public:
	GamePakView(GameController* controller, ConfigController* config, QWidget* parent = nullptr);

public slots:
	void saveOverride();

private slots:
	void updateOverrides();
	void gameStarted(GBAThread*);
	void gameStopped();
	void setLuminanceValue(int);

private:
	Ui::GamePakView m_ui;

	GameController* m_controller;
	ConfigController* m_config;
	GBACartridgeOverride m_override;
};

}

#endif
