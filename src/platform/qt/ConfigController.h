/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "Override.h"

#include <QMap>
#include <QObject>
#include <QSettings>
#include <QVariant>

#include <functional>

#include <mgba/core/config.h>
#include <mgba-util/configuration.h>
#include <mgba/feature/commandline.h>

class QMenu;

struct mArguments;
struct GBACartridgeOverride;

namespace QGBA {

class Action;
class ActionMapper;

class ConfigOption : public QObject {
Q_OBJECT

public:
	ConfigOption(const QString& name, QObject* parent = nullptr);

	void connect(std::function<void(const QVariant&)>, QObject* parent = nullptr);

	Action* addValue(const QString& text, const QVariant& value, ActionMapper* actions = nullptr, const QString& menu = {});
	Action* addValue(const QString& text, const char* value, ActionMapper* actions = nullptr, const QString& menu = {});
	Action* addBoolean(const QString& text, ActionMapper* actions = nullptr, const QString& menu = {});

	QString name() const { return m_name; }

public slots:
	void setValue(bool value);
	void setValue(int value);
	void setValue(unsigned value);
	void setValue(const char* value);
	void setValue(const QVariant& value);

signals:
	void valueChanged(const QVariant& value);

private:
	QMap<QObject*, std::function<void(const QVariant&)>> m_slots;
	QList<std::pair<Action*, QVariant>> m_actions;
	QString m_name;
};

class ConfigController : public QObject {
Q_OBJECT

public:
	constexpr static const char* const PORT = "qt";
	static const int MRU_LIST_SIZE = 10;

	ConfigController(QObject* parent = nullptr);
	~ConfigController();

	const mCoreOptions* options() const { return &m_opts; }
	bool parseArguments(mArguments* args, int argc, char* argv[], mSubParser* subparser = nullptr);

	ConfigOption* addOption(const char* key);
	void updateOption(const char* key);

	QString getOption(const char* key, const QVariant& defaultVal = QVariant()) const;
	QString getOption(const QString& key, const QVariant& defaultVal = QVariant()) const;

	QVariant getQtOption(const QString& key, const QString& group = QString()) const;

	QList<QString> getMRU() const;
	void setMRU(const QList<QString>& mru);

	Configuration* overrides() { return mCoreConfigGetOverrides(&m_config); }
	void saveOverride(const Override&);

	Configuration* input() { return mCoreConfigGetInput(&m_config); }

	const mCoreConfig* config() const { return &m_config; }
	mCoreConfig* config() { return &m_config; }

	static const QString& configDir();

public slots:
	void setOption(const char* key, bool value);
	void setOption(const char* key, int value);
	void setOption(const char* key, unsigned value);
	void setOption(const char* key, const char* value);
	void setOption(const char* key, const QVariant& value);
	void setQtOption(const QString& key, const QVariant& value, const QString& group = QString());

	void makePortable();
	void write();

private:
	Configuration* defaults() { return &m_config.defaultsTable; }

	mCoreConfig m_config;
	mCoreOptions m_opts{};

	QMap<QString, ConfigOption*> m_optionSet;
	QSettings* m_settings;
	static QString s_configDir;
};

}
