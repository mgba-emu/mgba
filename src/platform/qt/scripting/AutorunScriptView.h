/* Copyright (c) 2013-2025 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "ui_AutorunScriptView.h"

namespace QGBA {

class AutorunScriptModel;
class ScriptingController;

class AutorunScriptView : public QDialog {
Q_OBJECT

public:
	AutorunScriptView(AutorunScriptModel* model, ScriptingController* controller, QWidget* parent = nullptr);
	void removeScript(const QModelIndex&);

private slots:
	void addScript();
	void removeScript();
	void moveUp();
	void moveDown();

private:
	Ui::AutorunScriptView m_ui;
	ScriptingController* m_controller;
};

}
