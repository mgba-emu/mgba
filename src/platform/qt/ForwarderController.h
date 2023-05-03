/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QFile>
#include <QObject>

#include "ForwarderGenerator.h"

#include <memory>

class QNetworkReply;

namespace QGBA {

class ForwarderController : public QObject {
Q_OBJECT

public:
	enum Download : int {
		MANIFEST,
		BASE,
		FORWARDER_KIT
	};
	ForwarderController(QObject* parent = nullptr);

	void setGenerator(std::unique_ptr<ForwarderGenerator>&& generator);
	ForwarderGenerator* generator() { return m_generator.get(); }

	void setBaseFilename(const QString& path) { m_baseFilename = path; }
	void clearBaseFilename() { m_baseFilename = QString(); }
	QString baseFilename() const { return m_baseFilename; }

	QString channel() const { return m_channel; }
	bool inProgress() const { return m_inProgress; }

public slots:
	void startBuild(const QString& outFilename);

signals:
	void buildStarted(bool needsForwarderKit);
	void downloadStarted(Download which);
	void downloadComplete(Download which);
	void downloadProgress(Download which, qint64 bytesGotten, qint64 bytesTotal);
	void buildComplete();
	void buildFailed();

private slots:
	void gotManifest(QNetworkReply*);
	void gotBuild(QNetworkReply*);
	void gotForwarderKit(QNetworkReply*);

private:
	void downloadForwarderKit();
	void downloadManifest();
	void downloadBuild(const QUrl&);
	bool toolInstalled(const QString& tool);
	void cleanup();

	void connectReply(QNetworkReply*, Download, void (ForwarderController::*next)(QNetworkReply*));

	QString m_channel{"dev"};
	QString m_outFilename;
	std::unique_ptr<ForwarderGenerator> m_generator;
	QFile m_sourceFile;
	QString m_baseFilename;
	bool m_inProgress = false;
	QByteArray m_originalPath;
};

}
