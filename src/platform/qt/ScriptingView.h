/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "ui_ScriptingView.h"

namespace QGBA {

class ScriptingController;

class ScriptingView : public QMainWindow {
Q_OBJECT

public:
	ScriptingView(ScriptingController* controller, QWidget* parent = nullptr);

private slots:
	void submitRepl();
	void load();

private:
	QString getFilters() const;
	Ui::ScriptingView m_ui;

	ScriptingController* m_controller;
};

}
