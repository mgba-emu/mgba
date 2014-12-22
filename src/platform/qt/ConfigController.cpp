/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ConfigController.h"

#include "GameController.h"

#include <QAction>
#include <QMenu>

extern "C" {
#include "platform/commandline.h"
}

using namespace QGBA;

ConfigOption::ConfigOption(QObject* parent)
	: QObject(parent)
{
}

void ConfigOption::connect(std::function<void(const QVariant&)> slot) {
	m_slot = slot;
}

QAction* ConfigOption::addValue(const QString& text, const QVariant& value, QMenu* parent) {
	QAction* action = new QAction(text, parent);
	action->setCheckable(true);
	QObject::connect(action, &QAction::triggered, [this, value]() {
		emit valueChanged(value);
	});
	parent->addAction(action);
	m_actions.append(qMakePair(action, value));
	return action;
}

QAction* ConfigOption::addValue(const QString& text, const char* value, QMenu* parent) {
	return addValue(text, QString(value), parent);
}

QAction* ConfigOption::addBoolean(const QString& text, QMenu* parent) {
	QAction* action = new QAction(text, parent);
	action->setCheckable(true);
	QObject::connect(action, &QAction::triggered, [this, action]() {
		emit valueChanged(action->isChecked());
	});
	parent->addAction(action);
	m_actions.append(qMakePair(action, 1));
	return action;
}

void ConfigOption::setValue(bool value) {
	setValue(QVariant(value));
}

void ConfigOption::setValue(int value) {
	setValue(QVariant(value));
}

void ConfigOption::setValue(unsigned value) {
	setValue(QVariant(value));
}

void ConfigOption::setValue(const char* value) {
	setValue(QVariant(QString(value)));
}

void ConfigOption::setValue(const QVariant& value) {
	QPair<QAction*, QVariant> action;
	foreach(action, m_actions) {
		bool signalsEnabled = action.first->blockSignals(true);
		action.first->setChecked(value == action.second);
		action.first->blockSignals(signalsEnabled);
	}
	m_slot(value);
}

ConfigController::ConfigController(QObject* parent)
	: QObject(parent)
	, m_opts()
{
	GBAConfigInit(&m_config, PORT);

	m_opts.audioSync = GameController::AUDIO_SYNC;
	m_opts.videoSync = GameController::VIDEO_SYNC;
	m_opts.fpsTarget = 60;
	m_opts.audioBuffers = 2048;
	m_opts.logLevel = GBA_LOG_WARN | GBA_LOG_ERROR | GBA_LOG_FATAL;
	GBAConfigLoadDefaults(&m_config, &m_opts);
	GBAConfigLoad(&m_config);
	GBAConfigMap(&m_config, &m_opts);
}

ConfigController::~ConfigController() {
	write();

	GBAConfigDeinit(&m_config);
	GBAConfigFreeOpts(&m_opts);
}

bool ConfigController::parseArguments(GBAArguments* args, int argc, char* argv[]) {
	return ::parseArguments(args, &m_config, argc, argv, 0);
}

ConfigOption* ConfigController::addOption(const char* key) {
	QString optionName(key);

	if (m_optionSet.contains(optionName)) {
		return m_optionSet[optionName];
	}
	ConfigOption* newOption = new ConfigOption(this);
	m_optionSet[optionName] = newOption;
	connect(newOption, &ConfigOption::valueChanged, [this, key](const QVariant& value) {
		setOption(key, value);
	});
	return newOption;
}

void ConfigController::updateOption(const char* key) {
	if (!key) {
		return;
	}

	QString optionName(key);

	if (!m_optionSet.contains(optionName)) {
		return;
	}
	m_optionSet[optionName]->setValue(GBAConfigGetValue(&m_config, key));
}

void ConfigController::setOption(const char* key, bool value) {
	GBAConfigSetIntValue(&m_config, key, value);
	ConfigOption* option = m_optionSet[QString(key)];
	if (option) {
		option->setValue(value);
	}
}

void ConfigController::setOption(const char* key, int value) {
	GBAConfigSetIntValue(&m_config, key, value);
	ConfigOption* option = m_optionSet[QString(key)];
	if (option) {
		option->setValue(value);
	}
}

void ConfigController::setOption(const char* key, unsigned value) {
	GBAConfigSetUIntValue(&m_config, key, value);
	ConfigOption* option = m_optionSet[QString(key)];
	if (option) {
		option->setValue(value);
	}
}

void ConfigController::setOption(const char* key, const char* value) {
	GBAConfigSetValue(&m_config, key, value);
	ConfigOption* option = m_optionSet[QString(key)];
	if (option) {
		option->setValue(value);
	}
}

void ConfigController::setOption(const char* key, const QVariant& value) {
	if (value.type() == QVariant::Bool) {
		setOption(key, value.toBool());
		return;
	}
	QString stringValue(value.toString());
	setOption(key, stringValue.toLocal8Bit().constData());
}

void ConfigController::write() {
	GBAConfigSave(&m_config);
}
