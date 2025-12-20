/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#if defined(BUILD_GL) || defined(BUILD_GLES2)

#include <QDialog>

#include "ui_ShaderSelector.h"

struct mGLES2Shader;
class QFormLayout;
class QGridLayout;
struct VideoShader;

namespace QGBA {

class ConfigController;
class Display;

class ShaderSelector : public QDialog {
Q_OBJECT

public:
	ShaderSelector(Display* display, ConfigController* config, QWidget* parent = nullptr);
	~ShaderSelector();

public slots:
	void saveSettings();
	void refreshShaders();
	void clear();
	void revert();

private slots:
	void selectShader();
	void loadShader(const QString& path, bool saveToSettings = false);
	void clearShader(bool saveToSettings = false);
	void buttonPressed(QAbstractButton*);

signals:
	void saveSettingsRequested();
	void reset();
	void resetToDefault();

private:
	void addUniform(QGridLayout*, const QString& section, const QString& name, float* value, float min, float max, int y, int x);
	void addUniform(QGridLayout*, const QString& section, const QString& name, int* value, int min, int max, int y, int x);
	void addMatchingUniformRows(mGLES2Shader* shader, QFormLayout* layout, const QString& name, int pass, const QString& uniformName, bool addAll);
	void parseShaderIni(mGLES2Shader* shader, QFormLayout* layout, const QString& name, int pass, QIODevice* file);
	QWidget* makePage(mGLES2Shader*, const QString& name, int pass, bool defaultPage);

	Ui::ShaderSelector m_ui;
	Display* m_display;
	ConfigController* m_config;
	VideoShader* m_shaders;
	QString m_shaderPath;
};

}

#endif
