/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ForwarderController.h"

#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "ConfigController.h"

#include <mgba/core/version.h>
#include <mgba/feature/updater.h>

using namespace QGBA;

#ifdef Q_OS_WIN
const QChar LIST_SPLIT{';'};
const char* SUFFIX = ".exe";
#else
const QChar LIST_SPLIT{':'};
const char* SUFFIX = "";
#endif

ForwarderController::ForwarderController(QObject* parent)
	: QObject(parent)
	, m_netman(new QNetworkAccessManager(this))
{
	m_netman->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
	connect(this, &ForwarderController::buildFailed, this, [this]() {
		m_inProgress = false;
	});
	connect(this, &ForwarderController::buildComplete, this, [this]() {
		m_inProgress = false;
	});
}

void ForwarderController::setGenerator(std::unique_ptr<ForwarderGenerator>&& generator) {
	m_generator = std::move(generator);
	connect(m_generator.get(), &ForwarderGenerator::buildFailed, this, &ForwarderController::buildFailed);
	connect(m_generator.get(), &ForwarderGenerator::buildFailed, this, &ForwarderController::cleanup);
	connect(m_generator.get(), &ForwarderGenerator::buildComplete, this, &ForwarderController::buildComplete);
	connect(m_generator.get(), &ForwarderGenerator::buildComplete, this, &ForwarderController::cleanup);
}

void ForwarderController::startBuild(const QString& outFilename) {
	if (m_inProgress) {
		return;
	}
	m_inProgress = true;
	m_outFilename = outFilename;

	QStringList neededTools = m_generator->externalTools();
	for (const auto& tool : neededTools) {
		if (!toolInstalled(tool)) {
			downloadForwarderKit();
			return;
		}
	}
	downloadManifest();
}

void ForwarderController::downloadForwarderKit() {
	// TODO
	emit buildFailed();
}

void ForwarderController::downloadManifest() {
	QNetworkReply* reply = m_netman->get(QNetworkRequest(QUrl("https://mgba.io/latest.ini")));
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		gotManifest(reply);
	});
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
	connect(reply, &QNetworkReply::errorOccurred, this, [this, reply]() {
#else
	connect(reply, qOverload<>(&QNetworkReply::error), this, [this, reply]() {
#endif
		emit buildFailed();
	});
}

void ForwarderController::gotManifest(QNetworkReply* reply) {
	if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
		emit buildFailed();
		return;
	}

	QByteArray manifest = reply->readAll();
	QString platform = m_generator->systemName();

	mUpdaterContext context;
	if (!mUpdaterInit(&context, manifest.constData())) {
		emit buildFailed();
		return;
	}
	QString bucket = QLatin1String(mUpdaterGetBucket(&context));

	mUpdate update;
	mUpdaterGetUpdateForChannel(&context, platform.toUtf8().constData(), m_channel.toUtf8().constData(), &update);

	downloadBuild({bucket + update.path});
    mUpdaterDeinit(&context);
}

void ForwarderController::downloadBuild(const QUrl& url) {
	QString extension(QFileInfo(url.path()).suffix());
	// TODO: cache this
	QString configDir(ConfigController::cacheDir());
	m_sourceFile.setFileName(QString("%1/%2-%3-%4.%5").arg(configDir)
		.arg(projectName)
		.arg(m_generator->systemName())
		.arg(channel())
		.arg(extension));
	if (!m_sourceFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		emit buildFailed();
		return;		
	}
	QNetworkReply* reply = m_netman->get(QNetworkRequest(url));

	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		gotBuild(reply);
	});

	connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
		QByteArray data = reply->readAll();
		m_sourceFile.write(data);
	});
}

void ForwarderController::gotBuild(QNetworkReply* reply) {
	if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
		emit buildFailed();
		return;
	}

	QByteArray data = reply->readAll();
	m_sourceFile.write(data);
	m_sourceFile.close();
	m_generator->rebuild(m_sourceFile.fileName(), m_outFilename);
}

void ForwarderController::cleanup() {
	m_sourceFile.remove();
}

bool ForwarderController::toolInstalled(const QString& tool) {
	QByteArray arr = qgetenv("PATH");
	QStringList path = QString::fromUtf8(arr).split(LIST_SPLIT);
	for (QDir dir : path) {
		QFileInfo exe(dir, tool + SUFFIX);
		if (exe.isExecutable()) {
			return true;
		}
	}
	return false;
}
