/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "ui_ScriptingView.h"

namespace QGBA {

class ConfigController;
class ScriptingController;
class ScriptingTextBuffer;

class ScriptingView : public QMainWindow {
Q_OBJECT

public:
	ScriptingView(ScriptingController* controller, ConfigController* config, QWidget* parent = nullptr);

private slots:
	void submitRepl();
	void load();

	void addTextBuffer(ScriptingTextBuffer*);
	void selectBuffer(int);

private:
	QString getFilters() const;

	void appendMRU(const QString&);
	void updateMRU();

	Ui::ScriptingView m_ui;

	ConfigController* m_config;
	ScriptingController* m_controller;
	QList<ScriptingTextBuffer*> m_textBuffers;
	QStringList m_mruFiles;
};

}
