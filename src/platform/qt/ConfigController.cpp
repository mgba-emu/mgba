/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ConfigController.h"

#include "CoreController.h"

#include <QAction>
#include <QDir>
#include <QMenu>

#include <mgba/feature/commandline.h>

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
	for (QPair<QAction*, QVariant>& action : m_actions) {
		bool signalsEnabled = action.first->blockSignals(true);
		action.first->setChecked(value == action.second);
		action.first->blockSignals(signalsEnabled);
	}
	for (std::function<void(const QVariant&)>& slot : m_slots.values()) {
		slot(value);
	}
}

QString ConfigController::s_configDir;

ConfigController::ConfigController(QObject* parent)
	: QObject(parent)
{
	QString fileName = configDir();
	fileName.append(QDir::separator());
	fileName.append("qt.ini");
	m_settings = new QSettings(fileName, QSettings::IniFormat, this);

	mCoreConfigInit(&m_config, PORT);

	m_opts.audioSync = CoreController::AUDIO_SYNC;
	m_opts.videoSync = CoreController::VIDEO_SYNC;
	m_opts.fpsTarget = 60;
	m_opts.audioBuffers = 1536;
	m_opts.sampleRate = 44100;
	m_opts.volume = 0x100;
	m_opts.logLevel = mLOG_WARN | mLOG_ERROR | mLOG_FATAL;
	m_opts.rewindEnable = false;
	m_opts.rewindBufferCapacity = 300;
	m_opts.rewindSave = true;
	m_opts.useBios = true;
	m_opts.suspendScreensaver = true;
	m_opts.lockAspectRatio = true;
	mCoreConfigLoad(&m_config);
	mCoreConfigLoadDefaults(&m_config, &m_opts);
	mCoreConfigMap(&m_config, &m_opts);
}

ConfigController::~ConfigController() {
	mCoreConfigDeinit(&m_config);
	mCoreConfigFreeOpts(&m_opts);
}

bool ConfigController::parseArguments(mArguments* args, int argc, char* argv[], mSubParser* subparser) {
	if (::parseArguments(args, argc, argv, subparser)) {
		mCoreConfigFreeOpts(&m_opts);
		applyArguments(args, subparser, &m_config);
		mCoreConfigMap(&m_config, &m_opts);
		return true;
	}
	return false;
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
	m_optionSet[optionName]->setValue(mCoreConfigGetValue(&m_config, key));
}

QString ConfigController::getOption(const char* key, const QVariant& defaultVal) const {
	const char* val = mCoreConfigGetValue(&m_config, key);
	if (val) {
		return QString(val);
	}
	return defaultVal.toString();
}

QString ConfigController::getOption(const QString& key, const QVariant& defaultVal) const {
	return getOption(key.toUtf8().constData(), defaultVal);
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

void ConfigController::saveOverride(const Override& override) {
	override.save(overrides());
	write();
}

void ConfigController::setOption(const char* key, bool value) {
	mCoreConfigSetIntValue(&m_config, key, value);
	QString optionName(key);
	if (m_optionSet.contains(optionName)) {
		m_optionSet[optionName]->setValue(value);
	}
}

void ConfigController::setOption(const char* key, int value) {
	mCoreConfigSetIntValue(&m_config, key, value);
	QString optionName(key);
	if (m_optionSet.contains(optionName)) {
		m_optionSet[optionName]->setValue(value);
	}
}

void ConfigController::setOption(const char* key, unsigned value) {
	mCoreConfigSetUIntValue(&m_config, key, value);
	QString optionName(key);
	if (m_optionSet.contains(optionName)) {
		m_optionSet[optionName]->setValue(value);
	}
}

void ConfigController::setOption(const char* key, const char* value) {
	mCoreConfigSetValue(&m_config, key, value);
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
	mCoreConfigSave(&m_config);
	m_settings->sync();

	mCoreConfigFreeOpts(&m_opts);
	mCoreConfigMap(&m_config, &m_opts);
}

void ConfigController::makePortable() {
	mCoreConfigMakePortable(&m_config);

	QString fileName(configDir());
	fileName.append(QDir::separator());
	fileName.append("qt.ini");
	QSettings* settings2 = new QSettings(fileName, QSettings::IniFormat, this);
	for (const auto& key : m_settings->allKeys()) {
		settings2->setValue(key, m_settings->value(key));
	}
	delete m_settings;
	m_settings = settings2;
}

const QString& ConfigController::configDir() {
	if (s_configDir.isNull()) {
		char path[PATH_MAX];
		mCoreConfigDirectory(path, sizeof(path));
		s_configDir = QString::fromUtf8(path);
	}
	return s_configDir;
}
