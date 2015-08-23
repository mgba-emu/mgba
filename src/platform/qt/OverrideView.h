/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_OVERRIDE_VIEW
#define QGBA_OVERRIDE_VIEW

#include <QDialog>

#include "ui_OverrideView.h"

extern "C" {
#include "gba/supervisor/overrides.h"
}

struct GBAThread;

namespace QGBA {

class ConfigController;
class GameController;

class OverrideView : public QDialog {
Q_OBJECT

public:
	OverrideView(GameController* controller, ConfigController* config, QWidget* parent = nullptr);

public slots:
	void saveOverride();

private slots:
	void updateOverrides();
	void gameStarted(GBAThread*);
	void gameStopped();

private:
	Ui::OverrideView m_ui;

	GameController* m_controller;
	ConfigController* m_config;
	GBACartridgeOverride m_override;
};

}

#endif
