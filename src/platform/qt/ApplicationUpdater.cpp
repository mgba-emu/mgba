/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ApplicationUpdater.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

#include "ApplicationUpdatePrompt.h"
#include "ConfigController.h"
#include "GBAApp.h"

#include <mgba/core/version.h>
#include <mgba/feature/updater.h>
#include <mgba-util/table.h>

using namespace QGBA;

ApplicationUpdater::ApplicationUpdater(ConfigController* config, QObject* parent)
	: AbstractUpdater(parent)
	, m_config(config)
	, m_channel(currentChannel())
{
	QVariant lastCheck = config->getQtOption("lastUpdateCheck");
	if (lastCheck.isValid()) {
		m_lastCheck = lastCheck.toDateTime();
	}

	QByteArray bucket(m_config->getOption("update.bucket").toLatin1());
	if (!bucket.isNull()) {
		mUpdate lastUpdate;
		if (mUpdateLoad(m_config->config(), "update.stable", &lastUpdate)) {
			m_updates[QLatin1String("stable")] = UpdateInfo(bucket.constData(), &lastUpdate);
		}
		if (mUpdateLoad(m_config->config(), "update.dev", &lastUpdate)) {
			m_updates[QLatin1String("dev")] = UpdateInfo(bucket.constData(), &lastUpdate);
		}
	}

	connect(this, &AbstractUpdater::updateAvailable, this, [this, config](bool available) {
		m_lastCheck = QDateTime::currentDateTimeUtc();
		config->setQtOption("lastUpdateCheck", m_lastCheck);

		if (available && currentVersion() < updateInfo()) {
			ApplicationUpdatePrompt* prompt = new ApplicationUpdatePrompt;
			connect(prompt, &QDialog::accepted, GBAApp::app(), &GBAApp::restartForUpdate);
			prompt->setAttribute(Qt::WA_DeleteOnClose);
			prompt->show();
		}
	});

	connect(this, &AbstractUpdater::updateDone, this, [this, config]() {
		QByteArray arg0 = GBAApp::app()->arguments().at(0).toUtf8();
		QByteArray path = updateInfo().url.path().toUtf8();
		mUpdateRegister(config->config(), arg0.constData(), path.constData());
		config->write();
	});
}

QUrl ApplicationUpdater::manifestLocation() const {
	return {"https://mgba.io/latest.ini"};
}

QStringList ApplicationUpdater::listChannels() {
	QStringList channels;
	channels << QLatin1String("stable");
	channels << QLatin1String("dev");
	return channels;
}

QString ApplicationUpdater::currentChannel() {
	QString version(projectVersion);
	QString branch(gitBranch);
	QRegularExpression stable("^(?:(?:refs/)?(?:tags|heads)/)?[0-9]+\\.[0-9]+\\.[0-9]+$");
	if (branch.contains(stable) || (branch == "(unknown)" && version.contains(stable))) {
		return QLatin1String("stable");
	} else {
		return QLatin1String("dev");
	}
}

QString ApplicationUpdater::readableChannel(const QString& channel) {
	if (channel.isEmpty()) {
		return readableChannel(currentChannel());
	}
	if (channel == QLatin1String("stable")) {
		return tr("Stable");
	}
	if (channel == QLatin1String("dev")) {
		return tr("Development");
	}
	return tr("Unknown");
}

ApplicationUpdater::UpdateInfo ApplicationUpdater::currentVersion() {
	UpdateInfo info;
	info.version = QLatin1String(projectVersion);
	info.rev = gitRevision;
	info.commit = QLatin1String(gitCommit);
	info.size = 0;
	return info;
}

QUrl ApplicationUpdater::parseManifest(const QByteArray& manifest) {
	const char* bytes = manifest.constData();

	mUpdaterContext context;
	if (!mUpdaterInit(&context, bytes)) {
		return {};
	}
	m_bucket = QLatin1String(mUpdaterGetBucket(&context));
	m_config->setOption("update.bucket", m_bucket);

	Table updates;
	HashTableInit(&updates, 4, free);
	mUpdaterGetUpdates(&context, platform(), &updates);

	m_updates.clear();
	HashTableEnumerate(&updates, [](const char* key, void* value, void* user) {
		const mUpdate* update = static_cast<mUpdate*>(value);
		ApplicationUpdater* self = static_cast<ApplicationUpdater*>(user);
		self->m_updates[QString::fromUtf8(key)] = UpdateInfo(self->m_bucket, update);
		QByteArray prefix(QString("update.%1").arg(key).toUtf8());
		mUpdateRecord(self->m_config->config(), prefix.constData(), update);
	}, static_cast<void*>(this));

	HashTableDeinit(&updates);
	mUpdaterDeinit(&context);

	if (!m_updates.contains(m_channel)) {
		return {};
	}

	return m_updates[m_channel].url;
}

QString ApplicationUpdater::destination() const {
	QFileInfo path(updateInfo().url.path());
	QDir dir(ConfigController::cacheDir());
	// QFileInfo::completeSuffix will eat all .'s in the filename...including
	// ones in the version string, turning mGBA-1.0.0-win32.7z into
	// 0.0-win32.7z instead of the intended .7z
	// As a result, so we have to split out the complete suffix manually.
	QString suffix(path.suffix());
	if (path.completeBaseName().endsWith(".tar")) {
		suffix = "tar." + suffix;
	}
	return dir.filePath(QLatin1String("update.") + suffix);
}

const char* ApplicationUpdater::platform() {
#ifdef Q_OS_WIN
	QFileInfo exe(GBAApp::app()->arguments().at(0));
	QFileInfo uninstallInfo(exe.dir().filePath("unins000.dat"));
#ifdef Q_OS_WIN64
	return uninstallInfo.exists() ? "win64-installer" : "win64";
#elif defined(Q_OS_WIN32)
	return uninstallInfo.exists() ? "win32-installer" : "win32";
#endif
#elif defined(Q_OS_MACOS)
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
	// Modern macOS build
	return "macos";
#else
	// Legacy "OS X" build
	return "osx";
#endif
#elif defined(Q_OS_LINUX) && defined(__x86_64__)
	return "appimage-x64";
#else
	// Return one that will be up to date, but we can't download
	return "win64";
#endif
}

ApplicationUpdater::UpdateInfo::UpdateInfo(const QString& prefix, const mUpdate* update)
	: rev(-1)
	, size(update->size)
	, url(prefix + update->path)
{
	if (update->rev > 0) {
		rev = update->rev;
	}
	if (update->commit) {
		commit = update->commit;
	}
	if (update->version) {
		version = QLatin1String(update->version);
	}
	if (update->sha256) {
		sha256 = QByteArray::fromHex(update->sha256);
	}
}

bool ApplicationUpdater::UpdateInfo::operator<(const ApplicationUpdater::UpdateInfo& other) const {
	if (rev > 0 && other.rev > 0) {
		return rev < other.rev;
	}
	if (!version.isNull() && !other.version.isNull()) {
		QStringList components = version.split(QChar('.'));
		QStringList otherComponents = other.version.split(QChar('.'));
		for (int i = 0; i < std::max<int>(components.count(), otherComponents.count()); ++i) {
			int component = -1;
			int otherComponent = -1;
			if (i < components.count()) {
				bool ok = true;
				component = components[i].toInt(&ok);
				if (!ok) {
					return false;
				}
			}
			if (i < otherComponents.count()) {
				bool ok = true;
				otherComponent = otherComponents[i].toInt(&ok);
				if (!ok) {
					return false;
				}
			}
			if (component < otherComponent) {
				return true;
			}
		}
		return false;
	}
	return false;
}

ApplicationUpdater::UpdateInfo::operator QString() const {
	if (!version.isNull()) {
		return version;
	}
	if (rev <= 0) {
		return ApplicationUpdater::tr("(None)");
	}
	int len = strlen(gitCommitShort);
	const char* pos = strchr(gitCommitShort, '-');
	if (pos) {
		len = pos - gitCommitShort;
	}
	return QString("r%1-%2").arg(rev).arg(commit.left(len));
}
