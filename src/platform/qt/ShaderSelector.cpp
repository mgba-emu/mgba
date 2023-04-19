/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ShaderSelector.h"

#include "ConfigController.h"
#include "GBAApp.h"
#include "Display.h"
#include "VFileDevice.h"

#include <QCheckBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QSpinBox>

#include <mgba/core/version.h>
#include <mgba/feature/video-backend.h>
#include <mgba-util/vfs.h>

#if defined(BUILD_GL) || defined(BUILD_GLES2)

#if !defined(_WIN32) || defined(USE_EPOXY)
#include "platform/opengl/gles2.h"
#endif

using namespace QGBA;

ShaderSelector::ShaderSelector(Display* display, ConfigController* config, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
	, m_display(display)
	, m_config(config)
	, m_shaderPath(config->getOption("shader"))
{
	m_ui.setupUi(this);

	refreshShaders();

	connect(m_ui.load, &QAbstractButton::clicked, this, &ShaderSelector::selectShader);
	connect(m_ui.unload, &QAbstractButton::clicked, this, &ShaderSelector::clearShader);
	connect(m_ui.buttonBox, &QDialogButtonBox::clicked, this, &ShaderSelector::buttonPressed);
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

void ShaderSelector::selectShader() {
	QDir path(GBAApp::dataDir());
	path.cd(QLatin1String("shaders"));
	QString name = GBAApp::app()->getOpenDirectoryName(this, tr("Load shader"), path.absolutePath());
	if (!name.isNull()) {
		loadShader(name);
		refreshShaders();
	}
}

void ShaderSelector::loadShader(const QString& path) {
	VDir* shader = VFileDevice::openDir(path);
	if (!shader) {
		shader = VFileDevice::openArchive(path);
	}
	if (!shader) {
		return;
	}
	m_display->setShaders(shader);
	shader->close(shader);
	m_shaderPath = path;
	m_config->setOption("shader", m_shaderPath);
}

void ShaderSelector::clearShader() {
	m_display->clearShaders();
	refreshShaders();
	m_shaderPath = "";
	m_config->setOption("shader", m_shaderPath);
}

void ShaderSelector::refreshShaders() {
	clear();
	m_shaders = m_display->shaders();
	if (!m_shaders) {
		return;
	}
	if (m_shaders->name) {
		m_ui.shaderName->setText(m_shaders->name);
	} else {
		m_ui.shaderName->setText(tr("No shader loaded"));
	}
	if (m_shaders->description) {
		m_ui.description->setText(m_shaders->description);
	} else {
		m_ui.description->clear();
	}
	if (m_shaders->author) {
		m_ui.author->setText(tr("by %1").arg(m_shaders->author));
	} else {
		m_ui.author->clear();
	}

	disconnect(this, &ShaderSelector::saved, 0, 0);
	disconnect(this, &ShaderSelector::reset, 0, 0);
	disconnect(this, &ShaderSelector::resetToDefault, 0, 0);

#if !defined(_WIN32) || defined(USE_EPOXY)
	if (m_shaders->preprocessShader) {
		m_ui.passes->addTab(makePage(static_cast<mGLES2Shader*>(m_shaders->preprocessShader), "default", 0), tr("Preprocessing"));
	}
	mGLES2Shader* shaders = static_cast<mGLES2Shader*>(m_shaders->passes);
	QFileInfo fi(m_shaderPath);
	for (size_t p = 0; p < m_shaders->nPasses; ++p) {
		QWidget* page = makePage(&shaders[p], fi.baseName(), p);
		if (page) {
			m_ui.passes->addTab(page, tr("Pass %1").arg(p + 1));
		}
	}
#endif
}

void ShaderSelector::addUniform(QGridLayout* settings, const QString& section, const QString& name, float* value, float min, float max, int y, int x) {
	QDoubleSpinBox* f = new QDoubleSpinBox;
	f->setDecimals(3);
	if (min < max) {
		f->setMinimum(min);
		f->setMaximum(max);
	}
	float def = *value;
	bool ok = false;
	float v = m_config->getQtOption(name, section).toFloat(&ok);
	if (ok) {
		*value = v;
	}
	f->setValue(*value);
	f->setSingleStep(0.001);
	f->setAccelerated(true);
	settings->addWidget(f, y, x);
	connect(f, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), [value](double v) {
		*value = v;
	});
	connect(this, &ShaderSelector::saved, [this, section, name, f]() {
		m_config->setQtOption(name, f->value(), section);
	});
	connect(this, &ShaderSelector::reset, [this, section, name, f]() {
		bool ok = false;
		float v = m_config->getQtOption(name, section).toFloat(&ok);
		if (ok) {
			f->setValue(v);
		}
	});
	connect(this, &ShaderSelector::resetToDefault, [def, section, name, f]() {
		f->setValue(def);
	});
}

void ShaderSelector::addUniform(QGridLayout* settings, const QString& section, const QString& name, int* value, int min, int max, int y, int x) {
	QSpinBox* i = new QSpinBox;
	if (min < max) {
		i->setMinimum(min);
		i->setMaximum(max);
	}
	int def = *value;
	bool ok = false;
	int v = m_config->getQtOption(name, section).toInt(&ok);
	if (ok) {
		*value = v;
	}
	i->setValue(*value);
	i->setSingleStep(1);
	i->setAccelerated(true);
	settings->addWidget(i, y, x);
	connect(i, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [value](int v) {
		*value = v;
	});
	connect(this, &ShaderSelector::saved, [this, section, name, i]() {
		m_config->setQtOption(name, i->value(), section);
	});
	connect(this, &ShaderSelector::reset, [this, section, name, i]() {
		bool ok = false;
		int v = m_config->getQtOption(name, section).toInt(&ok);
		if (ok) {
			i->setValue(v);
		}
	});
	connect(this, &ShaderSelector::resetToDefault, [def, section, name, i]() {
		i->setValue(def);
	});
}

QWidget* ShaderSelector::makePage(mGLES2Shader* shader, const QString& name, int pass) {
#if !defined(_WIN32) || defined(USE_EPOXY)
	if (!shader->nUniforms) {
		return nullptr;
	}
	QWidget* page = new QWidget;
	QFormLayout* layout = new QFormLayout;
	page->setLayout(layout);
	for (size_t u = 0 ; u < shader->nUniforms; ++u) {
		QGridLayout* settings = new QGridLayout;
		mGLES2Uniform* uniform = &shader->uniforms[u];
		QString section = QString("shader.%1.%2").arg(name).arg(pass);
		QString name = QLatin1String(uniform->name);
		switch (uniform->type) {
		case GL_FLOAT:
			addUniform(settings, section, name, &uniform->value.f, uniform->min.f, uniform->max.f, 0, 0);
			break;
		case GL_FLOAT_VEC2:
			addUniform(settings, section, name + "[0]", &uniform->value.fvec2[0], uniform->min.fvec2[0], uniform->max.fvec2[0], 0, 0);
			addUniform(settings, section, name + "[1]", &uniform->value.fvec2[1], uniform->min.fvec2[1], uniform->max.fvec2[1], 0, 1);
			break;
		case GL_FLOAT_VEC3:
			addUniform(settings, section, name + "[0]", &uniform->value.fvec3[0], uniform->min.fvec3[0], uniform->max.fvec3[0], 0, 0);
			addUniform(settings, section, name + "[1]", &uniform->value.fvec3[1], uniform->min.fvec3[1], uniform->max.fvec3[1], 0, 1);
			addUniform(settings, section, name + "[2]", &uniform->value.fvec3[2], uniform->min.fvec3[2], uniform->max.fvec3[2], 0, 2);
			break;
		case GL_FLOAT_VEC4:
			addUniform(settings, section, name + "[0]", &uniform->value.fvec4[0], uniform->min.fvec4[0], uniform->max.fvec4[0], 0, 0);
			addUniform(settings, section, name + "[1]", &uniform->value.fvec4[1], uniform->min.fvec4[1], uniform->max.fvec4[1], 0, 1);
			addUniform(settings, section, name + "[2]", &uniform->value.fvec4[2], uniform->min.fvec4[2], uniform->max.fvec4[2], 0, 2);
			addUniform(settings, section, name + "[3]", &uniform->value.fvec4[3], uniform->min.fvec4[3], uniform->max.fvec4[3], 0, 3);
			break;
		case GL_INT:
			addUniform(settings, section, name, &uniform->value.i, uniform->min.i, uniform->max.i, 0, 0);
			break;
		case GL_INT_VEC2:
			addUniform(settings, section, name + "[0]", &uniform->value.ivec2[0], uniform->min.ivec2[0], uniform->max.ivec2[0], 0, 0);
			addUniform(settings, section, name + "[1]", &uniform->value.ivec2[1], uniform->min.ivec2[1], uniform->max.ivec2[1], 0, 1);
			break;
		case GL_INT_VEC3:
			addUniform(settings, section, name + "[0]", &uniform->value.ivec3[0], uniform->min.ivec3[0], uniform->max.ivec3[0], 0, 0);
			addUniform(settings, section, name + "[1]", &uniform->value.ivec3[1], uniform->min.ivec3[1], uniform->max.ivec3[1], 0, 1);
			addUniform(settings, section, name + "[2]", &uniform->value.ivec3[2], uniform->min.ivec3[2], uniform->max.ivec3[2], 0, 2);
			break;
		case GL_INT_VEC4:
			addUniform(settings, section, name + "[0]", &uniform->value.ivec4[0], uniform->min.ivec4[0], uniform->max.ivec4[0], 0, 0);
			addUniform(settings, section, name + "[1]", &uniform->value.ivec4[1], uniform->min.ivec4[1], uniform->max.ivec4[1], 0, 1);
			addUniform(settings, section, name + "[2]", &uniform->value.ivec4[2], uniform->min.ivec4[2], uniform->max.ivec4[2], 0, 2);
			addUniform(settings, section, name + "[3]", &uniform->value.ivec4[3], uniform->min.ivec4[3], uniform->max.ivec4[3], 0, 3);
			break;
		}
		layout->addRow(shader->uniforms[u].readableName, settings);
	}
	return page;
#else
	return nullptr;
#endif
}

void ShaderSelector::buttonPressed(QAbstractButton* button) {
	switch (m_ui.buttonBox->standardButton(button)) {
	case QDialogButtonBox::Reset:
		emit reset();
		break;
	case QDialogButtonBox::Ok:
		emit saved();
		close();
		break;
 	case QDialogButtonBox::RestoreDefaults:
		emit resetToDefault();
		break;
	default:
		break;
	}
}

#endif
