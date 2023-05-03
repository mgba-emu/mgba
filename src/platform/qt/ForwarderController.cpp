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
#include "GBAApp.h"
#include "VFileDevice.h"

#include <mgba/core/version.h>
#include <mgba/feature/updater.h>
#include <mgba-util/vfs.h>

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
	, m_originalPath(qgetenv("PATH"))
{
	connect(this, &ForwarderController::buildFailed, this, &ForwarderController::cleanup);
	connect(this, &ForwarderController::buildComplete, this, &ForwarderController::cleanup);
}

void ForwarderController::setGenerator(std::unique_ptr<ForwarderGenerator>&& generator) {
	m_generator = std::move(generator);
	connect(m_generator.get(), &ForwarderGenerator::buildFailed, this, &ForwarderController::buildFailed);
	connect(m_generator.get(), &ForwarderGenerator::buildComplete, this, &ForwarderController::buildComplete);
}

void ForwarderController::startBuild(const QString& outFilename) {
	if (m_inProgress) {
		return;
	}
	m_inProgress = true;
	m_outFilename = outFilename;

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
	// Amend the path for downloaded programs forwarder-kit
	QByteArray arr = m_originalPath;
	QStringList path = QString::fromUtf8(arr).split(LIST_SPLIT);
	path << ConfigController::cacheDir();
	arr = path.join(LIST_SPLIT).toUtf8();
	qputenv("PATH", arr);
#endif

	QStringList neededTools = m_generator->externalTools();
	for (const auto& tool : neededTools) {
		if (!toolInstalled(tool)) {
			downloadForwarderKit();
			return;
		}
	}
	if (m_baseFilename.isEmpty()) {
		downloadManifest();
	} else {
		m_generator->rebuild(m_baseFilename, m_outFilename);
	}
}

void ForwarderController::downloadForwarderKit() {
	QString fkUrl("https://github.com/mgba-emu/forwarder-kit/releases/latest/download/forwarder-kit-%1.zip");
#ifdef Q_OS_WIN64
	fkUrl = fkUrl.arg("win64");
#elif defined(Q_OS_WIN32)
	fkUrl = fkUrl.arg("win32");
#elif defined(Q_OS_MAC) && (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
	// Modern macOS build
	fkUrl = fkUrl.arg("macos");
#else
	// TODO
	emit buildFailed();
	return;
#endif
	QNetworkReply* reply = GBAApp::app()->httpGet(QUrl(fkUrl));
	connectReply(reply, FORWARDER_KIT, &ForwarderController::gotForwarderKit);
}

void ForwarderController::gotForwarderKit(QNetworkReply* reply) {
	emit downloadComplete(FORWARDER_KIT);
	if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
		emit buildFailed();
		return;
	}

	QFile fkZip(ConfigController::cacheDir() + "/forwarder-kit.zip");
	fkZip.open(QIODevice::WriteOnly | QIODevice::Truncate);
	QByteArray arr;
	do {
		arr = reply->read(0x800);
		fkZip.write(arr);
	} while (!arr.isEmpty());
	fkZip.close();

	VDir* fkDir = VFileDevice::openArchive(fkZip.fileName());

	// This has to be done in multiple passes to avoid seeking breaking the listing
	QStringList files;
	for (VDirEntry* entry = fkDir->listNext(fkDir); entry; entry = fkDir->listNext(fkDir)) {
		if (entry->type(entry) != VFS_FILE) {
			continue;
		}
		files << entry->name(entry);
	}

	for (const QString& source : files) {
		VFile* sourceVf = fkDir->openFile(fkDir, source.toUtf8().constData(), O_RDONLY);
		VFile* targetVf = VFileDevice::open(ConfigController::cacheDir() + "/" + source, O_CREAT | O_TRUNC | O_WRONLY);
		VFileDevice::copyFile(sourceVf, targetVf);
		sourceVf->close(sourceVf);
		targetVf->close(targetVf);

		QFile target(ConfigController::cacheDir() + "/" + source);
		target.setPermissions(target.permissions() | QFileDevice::ExeOwner |  QFileDevice::ExeUser);
	}

	fkDir->close(fkDir);
	fkZip.remove();

	downloadManifest();
}

void ForwarderController::downloadManifest() {
	QNetworkReply* reply = GBAApp::app()->httpGet(QUrl("https://mgba.io/latest.ini"));
	connectReply(reply, MANIFEST, &ForwarderController::gotManifest);
}

void ForwarderController::gotManifest(QNetworkReply* reply) {
	emit downloadComplete(MANIFEST);
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
	QNetworkReply* reply = GBAApp::app()->httpGet(url);

	connectReply(reply, BASE, &ForwarderController::gotBuild);
	connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
		if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() / 100 != 2) {
			return;
		}
		QByteArray data = reply->readAll();
		m_sourceFile.write(data);
	});
}

void ForwarderController::gotBuild(QNetworkReply* reply) {
	emit downloadComplete(BASE);
	if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
		emit buildFailed();
		return;
	}

	QByteArray data = reply->readAll();
	m_sourceFile.write(data);
	m_sourceFile.close();

	QString extracted = m_generator->extract(m_sourceFile.fileName());
	if (extracted.isNull()) {
		emit buildFailed();
		return;
	}
	m_generator->rebuild(extracted, m_outFilename);
}

void ForwarderController::cleanup() {
	if (m_sourceFile.exists()) {
		m_sourceFile.remove();
	}
	m_inProgress = false;

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
	qputenv("PATH", m_originalPath);
#endif
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

void ForwarderController::connectReply(QNetworkReply* reply, Download download, void (ForwarderController::*next)(QNetworkReply*)) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
	connect(reply, &QNetworkReply::errorOccurred, this, [this, reply]() {
#else
	connect(reply, qOverload<>(&QNetworkReply::error), this, [this, reply]() {
#endif
		emit buildFailed();
	});

	connect(reply, &QNetworkReply::finished, this, [this, reply, download, next]() {
		if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() / 100 == 3) {
			QNetworkReply* newReply = GBAApp::app()->httpGet(reply->header(QNetworkRequest::LocationHeader).toString());
			connectReply(newReply, download, next);
		} else {
			(this->*next)(reply);
		}
	});
	connect(reply, &QNetworkReply::downloadProgress, this, [this, download](qint64 bytesReceived, qint64 bytesTotal) {
		emit downloadProgress(download, bytesReceived, bytesTotal);
	});
	emit downloadStarted(download);
}
