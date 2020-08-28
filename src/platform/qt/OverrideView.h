/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QDialog>
#include <QTimer>

#include <memory>

#ifdef M_CORE_GB
#include <mgba/gb/interface.h>
#endif

#include "ColorPicker.h"
#include "Override.h"

#include "ui_OverrideView.h"

struct mCoreThread;

namespace QGBA {

class ConfigController;
class CoreController;
class Override;

class OverrideView : public QDialog {
Q_OBJECT

public:
	OverrideView(ConfigController* config, QWidget* parent = nullptr);

	void setController(std::shared_ptr<CoreController> controller);

public slots:
	void saveOverride();
	void recheck();

private slots:
	void updateOverrides();
	void gameStarted();
	void gameStopped();

private:
	Ui::OverrideView m_ui;

	std::shared_ptr<CoreController> m_controller;
	ConfigController* m_config;
	bool m_savePending = false;
	QTimer m_recheck;

#ifdef M_CORE_GB
	uint32_t m_gbColors[12]{};
	ColorPicker m_colorPickers[12];
#endif
};

}
