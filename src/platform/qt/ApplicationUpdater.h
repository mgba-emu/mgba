/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "AbstractUpdater.h"

#include <QDateTime>
#include <QHash>
#include <QUrl>

struct mUpdate;

namespace QGBA {

class ConfigController;

class ApplicationUpdater : public AbstractUpdater {
Q_OBJECT

public:
	struct UpdateInfo {
		UpdateInfo() = default;
		UpdateInfo(const QString& prefix, const mUpdate*);

		QString version;
		int rev;
		QString commit;
		size_t size;
		QUrl url;
		QByteArray sha256;

		bool operator<(const UpdateInfo&) const;
		operator QString() const;
	};

	ApplicationUpdater(ConfigController* config, QObject* parent = nullptr);

	static QStringList listChannels();
	void setChannel(const QString& channel) { m_channel = channel; }
	static QString currentChannel();
	static QString readableChannel(const QString& channel = {});

	QHash<QString, UpdateInfo> listUpdates() const { return m_updates; }
	UpdateInfo updateInfo() const { return m_updates[m_channel]; }
	static UpdateInfo currentVersion();

	QDateTime lastCheck() const { return m_lastCheck; }

	virtual QString destination() const override;

protected:
	virtual QUrl manifestLocation() const override;
	virtual QUrl parseManifest(const QByteArray&) override;

private:
	static const char* platform();

	ConfigController* m_config;
	QHash<QString, UpdateInfo> m_updates;
	QString m_channel;
	QString m_bucket;
	QDateTime m_lastCheck;
};

}
