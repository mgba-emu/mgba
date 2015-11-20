/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_SHADER_SELECTOR_H
#define QGBA_SHADER_SELECTOR_H

#include <QDialog>

#include "ui_ShaderSelector.h"

class QGridLayout;
struct VideoShader;

namespace QGBA {

class Display;

class ShaderSelector : public QDialog {
Q_OBJECT

public:
	ShaderSelector(Display* display, QWidget* parent = nullptr);
	~ShaderSelector();

public slots:
	void refreshShaders();
	void clear();

private:
	void addUniform(QGridLayout*, float* value, float min, float max, int y, int x);
	void addUniform(QGridLayout*, int* value, int min, int max, int y, int x);

	Ui::ShaderSelector m_ui;
	Display* m_display;
	VideoShader* m_shaders;
};

}

#endif
