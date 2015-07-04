/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ConfigController.h"

#include "GameController.h"

#include <QAction>
#include <QDir>
#include <QMenu>

extern "C" {
#include "gba/supervisor/overrides.h"
#include "platform/commandline.h"
}

using namespace QGBA;

ConfigOption::ConfigOption(QObject* parent)
	: QObject(parent)
{
}

void ConfigOption::connect(std::function<void(const QVariant&)> slot, QObject* parent) {
	m_slots[parent] = slot;
	QObject::connect(parent, &QAction::destroyed, [this, slot, parent]() {
		m_slots.remove(parent);
	});
}

QAction* ConfigOption::addValue(const QString& text, const QVariant& value, QMenu* parent) {
	QAction* action = new QAction(text, parent);
	action->setCheckable(true);
	QObject::connect(action, &QAction::triggered, [this, value]() {
		emit valueChanged(value);
	});
	QObject::connect(parent, &QAction::destroyed, [this, action, value]() {
		m_actions.removeAll(qMakePair(action, value));
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
	QObject::connect(parent, &QAction::destroyed, [this, action]() {
		m_actions.removeAll(qMakePair(action, 1));
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
	foreach (action, m_actions) {
		bool signalsEnabled = action.first->blockSignals(true);
		action.first->setChecked(value == action.second);
		action.first->blockSignals(signalsEnabled);
	}
	std::function<void(const QVariant&)> slot;
	foreach(slot, m_slots.values()) {
		slot(value);
	}
}

ConfigController::ConfigController(QObject* parent)
	: QObject(parent)
	, m_opts()
{
	char path[PATH_MAX];
	GBAConfigDirectory(path, sizeof(path));
	QString fileName(path);
	fileName.append(QDir::separator());
	fileName.append("qt.ini");
	m_settings = new QSettings(fileName, QSettings::IniFormat, this);

	GBAConfigInit(&m_config, PORT);

	m_opts.audioSync = GameController::AUDIO_SYNC;
	m_opts.videoSync = GameController::VIDEO_SYNC;
	m_opts.fpsTarget = 60;
	m_opts.audioBuffers = 2048;
	m_opts.volume = GBA_AUDIO_VOLUME_MAX;
	m_opts.logLevel = GBA_LOG_WARN | GBA_LOG_ERROR | GBA_LOG_FATAL | GBA_LOG_STATUS;
	m_opts.rewindEnable = false;
	m_opts.rewindBufferInterval = 0;
	m_opts.rewindBufferCapacity = 0;
	m_opts.useBios = true;
	m_opts.suspendScreensaver = true;
	GBAConfigLoadDefaults(&m_config, &m_opts);
	GBAConfigLoad(&m_config);
	GBAConfigMap(&m_config, &m_opts);
}

ConfigController::~ConfigController() {
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

QString ConfigController::getOption(const char* key) const {
	return QString(GBAConfigGetValue(&m_config, key));
}

QVariant ConfigController::getQtOption(const QString& key, const QString& group) const {
	if (!group.isNull()) {
		m_settings->beginGroup(group);
	}
	QVariant value = m_settings->value(key);
	if (!group.isNull()) {
		m_settings->endGroup();
	}
	return value;
}

void ConfigController::saveOverride(const GBACartridgeOverride& override) {
	GBAOverrideSave(overrides(), &override);
	write();
}

void ConfigController::setOption(const char* key, bool value) {
	GBAConfigSetIntValue(&m_config, key, value);
	QString optionName(key);
	if (m_optionSet.contains(optionName)) {
		m_optionSet[optionName]->setValue(value);
	}
}

void ConfigController::setOption(const char* key, int value) {
	GBAConfigSetIntValue(&m_config, key, value);
	QString optionName(key);
	if (m_optionSet.contains(optionName)) {
		m_optionSet[optionName]->setValue(value);
	}
}

void ConfigController::setOption(const char* key, unsigned value) {
	GBAConfigSetUIntValue(&m_config, key, value);
	QString optionName(key);
	if (m_optionSet.contains(optionName)) {
		m_optionSet[optionName]->setValue(value);
	}
}

void ConfigController::setOption(const char* key, const char* value) {
	GBAConfigSetValue(&m_config, key, value);
	QString optionName(key);
	if (m_optionSet.contains(optionName)) {
		m_optionSet[optionName]->setValue(value);
	}
}

void ConfigController::setOption(const char* key, const QVariant& value) {
	if (value.type() == QVariant::Bool) {
		setOption(key, value.toBool());
		return;
	}
	QString stringValue(value.toString());
	setOption(key, stringValue.toUtf8().constData());
}

void ConfigController::setQtOption(const QString& key, const QVariant& value, const QString& group) {
	if (!group.isNull()) {
		m_settings->beginGroup(group);
	}
	m_settings->setValue(key, value);
	if (!group.isNull()) {
		m_settings->endGroup();
	}
}

QList<QString> ConfigController::getMRU() const {
	QList<QString> mru;
	m_settings->beginGroup("mru");
	for (int i = 0; i < MRU_LIST_SIZE; ++i) {
		QString item = m_settings->value(QString::number(i)).toString();
		if (item.isNull()) {
			continue;
		}
		mru.append(item);
	}
	m_settings->endGroup();
	return mru;
}

void ConfigController::setMRU(const QList<QString>& mru) {
	int i = 0;
	m_settings->beginGroup("mru");
	for (const QString& item : mru) {
		m_settings->setValue(QString::number(i), item);
		++i;
		if (i >= MRU_LIST_SIZE) {
			break;
		}
	}
	m_settings->endGroup();
}

void ConfigController::write() {
	GBAConfigSave(&m_config);
	m_settings->sync();
}
