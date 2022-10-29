/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ForwarderController.h"

#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "ConfigController.h"

#include <mgba/core/version.h>
#include <mgba/feature/updater.h>

using namespace QGBA;

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

void ForwarderController::startBuild(const QString& outFilename) {
	if (m_inProgress) {
		return;
	}
	m_inProgress = true;
	m_outFilename = outFilename;
	downloadManifest();
}

void ForwarderController::downloadManifest() {
	QNetworkReply* reply = m_netman->get(QNetworkRequest(QUrl("https://mgba.io/latest.ini")));
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		gotManifest(reply);
	});
	connect(reply, &QNetworkReply::errorOccurred, this, [this, reply]() {
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
	QString configDir(ConfigController::configDir());
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
	if (!m_generator->rebuild(m_sourceFile.fileName(), m_outFilename)) {
		emit buildFailed();
	} else {
		emit buildComplete();
	}
	m_sourceFile.remove();
}
