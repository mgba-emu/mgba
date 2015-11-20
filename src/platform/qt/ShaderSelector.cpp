/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ShaderSelector.h"

#include "Display.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QSpinBox>

extern "C" {
#include "platform/video-backend.h"

#if !defined(_WIN32) || defined(USE_EPOXY)
#include "platform/opengl/gles2.h"
#endif
}

using namespace QGBA;

ShaderSelector::ShaderSelector(Display* display, QWidget* parent)
	: QDialog(parent)
	, m_display(display)
{
	m_ui.setupUi(this);

	refreshShaders();
}

ShaderSelector::~ShaderSelector() {
	clear();
}

void ShaderSelector::clear() {
	m_ui.shaderName->setText(tr("No shader active"));
	m_ui.description->clear();
	m_ui.author->clear();

	while (QWidget* page = m_ui.passes->widget(0)) {
		m_ui.passes->removeTab(0);
		delete page;
	}
}

void ShaderSelector::refreshShaders() {
	clear();
	m_shaders = m_display->shaders();
	if (!m_shaders) {
		return;
	}
	m_ui.shaderName->setText(m_shaders->name);
	m_ui.description->setText(m_shaders->description);
	m_ui.author->setText(tr("by %1").arg(m_shaders->author));

#if !defined(_WIN32) || defined(USE_EPOXY)
	GBAGLES2Shader* shaders = static_cast<GBAGLES2Shader*>(m_shaders->passes);
	for (size_t p = 0; p < m_shaders->nPasses; ++p) {
		QWidget* page = new QWidget;
		QFormLayout* layout = new QFormLayout;
		page->setLayout(layout);
		for (size_t u = 0 ; u < shaders[p].nUniforms; ++u) {
			QGridLayout* settings = new QGridLayout;
			GBAGLES2Uniform* uniform = &shaders[p].uniforms[u];
			switch (uniform->type) {
			case GL_FLOAT:
				addUniform(settings, &uniform->value.f, uniform->min.f, uniform->max.f, 0, 0);
				break;
			case GL_FLOAT_VEC2:
				addUniform(settings, &uniform->value.fvec2[0], uniform->min.fvec2[0], uniform->max.fvec2[0], 0, 0);
				addUniform(settings, &uniform->value.fvec2[1], uniform->min.fvec2[1], uniform->max.fvec2[1], 0, 1);
				break;
			case GL_FLOAT_VEC3:
				addUniform(settings, &uniform->value.fvec3[0], uniform->min.fvec3[0], uniform->max.fvec3[0], 0, 0);
				addUniform(settings, &uniform->value.fvec3[1], uniform->min.fvec3[1], uniform->max.fvec3[1], 0, 1);
				addUniform(settings, &uniform->value.fvec3[2], uniform->min.fvec3[2], uniform->max.fvec3[2], 0, 2);
				break;
			case GL_FLOAT_VEC4:
				addUniform(settings, &uniform->value.fvec4[0], uniform->min.fvec4[0], uniform->max.fvec4[0], 0, 0);
				addUniform(settings, &uniform->value.fvec4[1], uniform->min.fvec4[1], uniform->max.fvec4[1], 0, 1);
				addUniform(settings, &uniform->value.fvec4[2], uniform->min.fvec4[2], uniform->max.fvec4[2], 0, 2);
				addUniform(settings, &uniform->value.fvec4[3], uniform->min.fvec4[3], uniform->max.fvec4[3], 0, 3);
				break;
			case GL_INT:
				addUniform(settings, &uniform->value.i, uniform->min.i, uniform->max.i, 0, 0);
				break;
			case GL_INT_VEC2:
				addUniform(settings, &uniform->value.ivec2[0], uniform->min.ivec2[0], uniform->max.ivec2[0], 0, 0);
				addUniform(settings, &uniform->value.ivec2[1], uniform->min.ivec2[1], uniform->max.ivec2[1], 0, 1);
				break;
			case GL_INT_VEC3:
				addUniform(settings, &uniform->value.ivec3[0], uniform->min.ivec3[0], uniform->max.ivec3[0], 0, 0);
				addUniform(settings, &uniform->value.ivec3[1], uniform->min.ivec3[1], uniform->max.ivec3[1], 0, 1);
				addUniform(settings, &uniform->value.ivec3[2], uniform->min.ivec3[2], uniform->max.ivec3[2], 0, 2);
				break;
			case GL_INT_VEC4:
				addUniform(settings, &uniform->value.ivec4[0], uniform->min.ivec4[0], uniform->max.ivec4[0], 0, 0);
				addUniform(settings, &uniform->value.ivec4[1], uniform->min.ivec4[1], uniform->max.ivec4[1], 0, 1);
				addUniform(settings, &uniform->value.ivec4[2], uniform->min.ivec4[2], uniform->max.ivec4[2], 0, 2);
				addUniform(settings, &uniform->value.ivec4[3], uniform->min.ivec4[3], uniform->max.ivec4[3], 0, 3);
				break;
			}
			layout->addRow(shaders[p].uniforms[u].readableName, settings);
		}
		m_ui.passes->addTab(page, tr("Pass %1").arg(p + 1));
	}
#endif
}

void ShaderSelector::addUniform(QGridLayout* settings, float* value, float min, float max, int y, int x) {
	QDoubleSpinBox* f = new QDoubleSpinBox;
	f->setDecimals(3);
	if (min < max) {
		f->setMinimum(min);
		f->setMaximum(max);
	}
	f->setValue(*value);
	f->setSingleStep(0.001);
	f->setAccelerated(true);
	settings->addWidget(f, y, x);
	connect(f, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), [value](double v) {
		*value = v;
	});
}

void ShaderSelector::addUniform(QGridLayout* settings, int* value, int min, int max, int y, int x) {
	QSpinBox* i = new QSpinBox;
	if (min < max) {
		i->setMinimum(min);
		i->setMaximum(max);
	}
	i->setValue(*value);
	i->setSingleStep(1);
	i->setAccelerated(true);
	settings->addWidget(i, y, x);
	connect(i, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [value](int v) {
		*value = v;
	});
}
