/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QByteArray>
#include <QFile>
#include <QObject>

class QNetworkReply;

namespace QGBA {

class AbstractUpdater : public QObject {
Q_OBJECT

public:
	AbstractUpdater(QObject* parent = nullptr);
	virtual ~AbstractUpdater() {}

public slots:
	void checkUpdate();
	void downloadUpdate();

signals:
	void updateAvailable(bool);
	void updateDone(bool);
	void updateProgress(float done);

protected:
	virtual QUrl manifestLocation() const = 0;
	virtual QUrl parseManifest(const QByteArray&) = 0;
	virtual QString destination() const = 0;

private slots:
	void progress(qint64 progress, qint64 max);

private:
	void chaseRedirects(QNetworkReply*, void (AbstractUpdater::*cb)(QNetworkReply*));
	void manifestDownloaded(QNetworkReply*);
	void updateDownloaded(QNetworkReply*);

	bool m_isUpdating = false;
	QByteArray m_manifest;
};

}
