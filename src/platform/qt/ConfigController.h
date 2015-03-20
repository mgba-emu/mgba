/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_CONFIG_CONTROLLER
#define QGBA_CONFIG_CONTROLLER

#include <QMap>
#include <QObject>
#include <QSettings>
#include <QVariant>

#include <functional>

extern "C" {
#include "gba/supervisor/config.h"
#include "util/configuration.h"
}

class QAction;
class QMenu;

struct GBAArguments;
struct GBACartridgeOverride;

namespace QGBA {

class ConfigOption : public QObject {
Q_OBJECT

public:
	ConfigOption(QObject* parent = nullptr);

	void connect(std::function<void(const QVariant&)>, QObject* parent = nullptr);

	QAction* addValue(const QString& text, const QVariant& value, QMenu* parent = nullptr);
	QAction* addValue(const QString& text, const char* value, QMenu* parent = nullptr);
	QAction* addBoolean(const QString& text, QMenu* parent = nullptr);

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
	QList<QPair<QAction*, QVariant>> m_actions;
};

class ConfigController : public QObject {
Q_OBJECT

public:
	constexpr static const char* const PORT = "qt";
	static const int MRU_LIST_SIZE = 10;

	ConfigController(QObject* parent = nullptr);
	~ConfigController();

	const GBAOptions* options() const { return &m_opts; }
	bool parseArguments(GBAArguments* args, int argc, char* argv[]);

	ConfigOption* addOption(const char* key);
	void updateOption(const char* key);

	QString getOption(const char* key) const;

	QVariant getQtOption(const QString& key, const QString& group = QString()) const;

	QList<QString> getMRU() const;
	void setMRU(const QList<QString>& mru);

	Configuration* overrides() { return GBAConfigGetOverrides(&m_config); }
	void saveOverride(const GBACartridgeOverride&);

public slots:
	void setOption(const char* key, bool value);
	void setOption(const char* key, int value);
	void setOption(const char* key, unsigned value);
	void setOption(const char* key, const char* value);
	void setOption(const char* key, const QVariant& value);
	void setQtOption(const QString& key, const QVariant& value, const QString& group = QString());

	void write();

private:
	Configuration* configuration() { return &m_config.configTable; }
	Configuration* defaults() { return &m_config.defaultsTable; }

	friend class InputController; // TODO: Do this without friends

	GBAConfig m_config;
	GBAOptions m_opts;

	QMap<QString, ConfigOption*> m_optionSet;
	QSettings* m_settings;
};

}

#endif
