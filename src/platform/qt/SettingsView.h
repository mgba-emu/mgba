/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_SETTINGS_VIEW
#define QGBA_SETTINGS_VIEW

#include <QDialog>

#include <mgba/core/core.h>

#include "ui_SettingsView.h"

namespace QGBA {

class ConfigController;
class InputController;
class ShortcutController;
class ShaderSelector;

class SettingsView : public QDialog {
Q_OBJECT

public:
	SettingsView(ConfigController* controller, InputController* inputController, ShortcutController* shortcutController, QWidget* parent = nullptr);
	~SettingsView();

	void setShaderSelector(ShaderSelector* shaderSelector);

signals:
	void biosLoaded(int platform, const QString&);
	void audioDriverChanged();
	void displayDriverChanged();
	void pathsChanged();
	void languageChanged();
	void libraryCleared();

private slots:
	void selectBios(QLineEdit*);
	void updateConfig();
	void reloadConfig();

private:
	Ui::SettingsView m_ui;

	ConfigController* m_controller;
	InputController* m_input;
	ShaderSelector* m_shader = nullptr;

	void saveSetting(const char* key, const QAbstractButton*);
	void saveSetting(const char* key, const QComboBox*);
	void saveSetting(const char* key, const QDoubleSpinBox*);
	void saveSetting(const char* key, const QLineEdit*);
	void saveSetting(const char* key, const QSlider*);
	void saveSetting(const char* key, const QSpinBox*);
	void saveSetting(const char* key, const QVariant&);

	void loadSetting(const char* key, QAbstractButton*);
	void loadSetting(const char* key, QComboBox*);
	void loadSetting(const char* key, QDoubleSpinBox*);
	void loadSetting(const char* key, QLineEdit*);
	void loadSetting(const char* key, QSlider*);
	void loadSetting(const char* key, QSpinBox*);
	QString loadSetting(const char* key);
};

}

#endif
