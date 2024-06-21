/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GBAApp.h"

#include "AudioProcessor.h"
#include "CoreController.h"
#include "CoreManager.h"
#include "ConfigController.h"
#include "Display.h"
#include "LogController.h"
#include "VFileDevice.h"
#include "Window.h"

#include <QFileInfo>
#include <QFileOpenEvent>
#include <QFontDatabase>
#include <QIcon>

#include <mgba/core/version.h>
#include <mgba/feature/updater.h>
#include <mgba-util/socket.h>
#include <mgba-util/vfs.h>

#ifdef USE_SQLITE3
#include "feature/sqlite3/no-intro.h"
#endif

#ifdef USE_DISCORD_RPC
#include "DiscordCoordinator.h"
#endif

#ifdef BUILD_SDL
#include "input/SDLInputDriver.h"
#endif

using namespace QGBA;

static GBAApp* g_app = nullptr;

mLOG_DEFINE_CATEGORY(QT, "Qt", "platform.qt");

GBAApp::GBAApp(int& argc, char* argv[], ConfigController* config)
	: QApplication(argc, argv)
	, m_configController(config)
	, m_updater(config)
	, m_monospace(QFontDatabase::systemFont(QFontDatabase::FixedFont))
{
	g_app = this;

#ifdef BUILD_SDL
	SDL_Init(SDL_INIT_NOPARACHUTE);
#endif

	SocketSubsystemInit();
	qRegisterMetaType<const uint32_t*>("const uint32_t*");
	qRegisterMetaType<mCoreThread*>("mCoreThread*");

	if (!m_configController->getQtOption("displayDriver").isNull()) {
		Display::setDriver(static_cast<Display::Driver>(m_configController->getQtOption("displayDriver").toInt()));
	}

	reloadGameDB();

	m_manager.setConfig(m_configController->config());
	m_manager.setMultiplayerController(&m_multiplayer);

	if (!m_configController->getQtOption("audioDriver").isNull()) {
		AudioProcessor::setDriver(static_cast<AudioProcessor::Driver>(m_configController->getQtOption("audioDriver").toInt()));
	}

	LogController::global()->load(m_configController);

#ifdef USE_DISCORD_RPC
	ConfigOption* useDiscordPresence = m_configController->addOption("useDiscordPresence");
	useDiscordPresence->addBoolean(tr("Enable Discord Rich Presence"));
	useDiscordPresence->connect([](const QVariant& value) {
		if (value.toBool()) {
			DiscordCoordinator::init();
		} else {
			DiscordCoordinator::deinit();
		}
	}, this);
	m_configController->updateOption("useDiscordPresence");
#endif

#if (QT_VERSION >= QT_VERSION_CHECK(5, 9, 0))
	m_netman.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
#endif

	cleanupAfterUpdate();

	connect(this, &GBAApp::aboutToQuit, this, &GBAApp::cleanup);
	if (m_configController->getOption("updateAutoCheck", 0).toInt()) {
		QMetaObject::invokeMethod(&m_updater, "checkUpdate", Qt::QueuedConnection);
	}
}

void GBAApp::cleanup() {
	m_workerThreads.waitForDone();

	while (!m_workerJobs.isEmpty()) {
		finishJob(m_workerJobs.firstKey());
	}

#ifdef USE_SQLITE3
	if (m_db) {
		NoIntroDBDestroy(m_db);
	}
#endif

#ifdef USE_DISCORD_RPC
	DiscordCoordinator::deinit();
#endif
}

bool GBAApp::event(QEvent* event) {
	if (event->type() == QEvent::FileOpen) {
		CoreController* core = m_manager.loadGame(static_cast<QFileOpenEvent*>(event)->file());
		m_windows[0]->setController(core, static_cast<QFileOpenEvent*>(event)->file());
		return true;
	}
	return QApplication::event(event);
}

Window* GBAApp::newWindow() {
	if (m_windows.count() >= MAX_GBAS) {
		return nullptr;
	}
	Window* w = new Window(&m_manager, m_configController, m_windows.count());
	connect(w, &Window::destroyed, [this, w]() {
		m_windows.removeAll(w);
		for (Window* w : m_windows) {
			w->updateMultiplayerStatus(m_windows.count() < MAX_GBAS);
		}
	});
	m_windows.append(w);
	w->setAttribute(Qt::WA_DeleteOnClose);
	w->loadConfig();
	w->show();
	w->multiplayerChanged();
	for (Window* w : m_windows) {
		w->updateMultiplayerStatus(m_windows.count() < MAX_GBAS);
	}
	return w;
}

GBAApp* GBAApp::app() {
	return g_app;
}

void GBAApp::pauseAll(QList<Window*>* paused) {
	for (auto& window : m_windows) {
		if (!window->controller() || window->controller()->isPaused()) {
			continue;
		}
		window->controller()->setPaused(true);
		paused->append(window);
	}
}

void GBAApp::continueAll(const QList<Window*>& paused) {
	for (auto& window : paused) {
		if (window->controller()) {
			window->controller()->setPaused(false);
		}
	}
}

QString GBAApp::getOpenFileName(QWidget* owner, const QString& title, const QString& filter) {
	QList<Window*> paused;
	pauseAll(&paused);
	QString filename = QFileDialog::getOpenFileName(owner, title, m_configController->getOption("lastDirectory"), filter);
	continueAll(paused);
	if (!filename.isEmpty()) {
		m_configController->setOption("lastDirectory", QFileInfo(filename).dir().canonicalPath());
	}
	return filename;
}

QStringList GBAApp::getOpenFileNames(QWidget* owner, const QString& title, const QString& filter) {
	QList<Window*> paused;
	pauseAll(&paused);
	QStringList filenames = QFileDialog::getOpenFileNames(owner, title, m_configController->getOption("lastDirectory"), filter);
	continueAll(paused);
	if (!filenames.isEmpty()) {
		m_configController->setOption("lastDirectory", QFileInfo(filenames.at(0)).dir().canonicalPath());
	}
	return filenames;
}

QString GBAApp::getSaveFileName(QWidget* owner, const QString& title, const QString& filter) {
	QList<Window*> paused;
	pauseAll(&paused);
	QString filename = QFileDialog::getSaveFileName(owner, title, m_configController->getOption("lastDirectory"), filter);
	continueAll(paused);
	if (!filename.isEmpty()) {
		m_configController->setOption("lastDirectory", QFileInfo(filename).dir().canonicalPath());
	}
	return filename;
}

QString GBAApp::getOpenDirectoryName(QWidget* owner, const QString& title, const QString& path) {
	QList<Window*> paused;
	pauseAll(&paused);
	QString filename = QFileDialog::getExistingDirectory(owner, title, !path.isNull() ? path : m_configController->getOption("lastDirectory"));
	continueAll(paused);
	if (path.isNull() && !filename.isEmpty()) {
		m_configController->setOption("lastDirectory", QFileInfo(filename).dir().canonicalPath());
	}
	return filename;
}

QString GBAApp::dataDir() {
#ifdef DATADIR
	QString path = QString::fromUtf8(DATADIR);
	if (path.startsWith("./") || path.startsWith("../")) {
		path = QCoreApplication::applicationDirPath() + "/" + path;
	}
#else
	QString path = QCoreApplication::applicationDirPath();
#ifdef Q_OS_MAC
	path += QLatin1String("/../Resources");
#endif
#endif
	return path;
}

#ifdef USE_SQLITE3
bool GBAApp::reloadGameDB() {
	NoIntroDB* db = nullptr;
	db = NoIntroDBLoad((ConfigController::configDir() + "/nointro.sqlite3").toUtf8().constData());
	if (db && m_db) {
		NoIntroDBDestroy(m_db);
	}
	if (db) {
		std::shared_ptr<GameDBParser> parser = std::make_shared<GameDBParser>(db);
		submitWorkerJob(std::bind(&GameDBParser::parseNoIntroDB, parser));
		m_db = db;
		return true;
	}
	return false;
}
#else
bool GBAApp::reloadGameDB() {
	return false;
}
#endif

QNetworkAccessManager* GBAApp::netman() {
	return &m_netman;
}

QNetworkReply* GBAApp::httpGet(const QUrl& url) {
	QNetworkRequest req(url);
	req.setHeader(QNetworkRequest::UserAgentHeader,
	              QString("%1/%2 (+https://mgba.io) is definitely not Mozilla/5.0")
	              .arg(projectName)
	              .arg(projectVersion));
	return m_netman.get(req);
}

qint64 GBAApp::submitWorkerJob(std::function<void ()> job, std::function<void ()> callback) {
	return submitWorkerJob(job, nullptr, callback);
}

qint64 GBAApp::submitWorkerJob(std::function<void ()> job, QObject* context, std::function<void ()> callback) {
	qint64 jobId = m_nextJob;
	++m_nextJob;
	WorkerJob* jobRunnable = new WorkerJob(jobId, job, this);
	m_workerJobs.insert(jobId, jobRunnable);
	if (callback) {
		waitOnJob(jobId, context, callback);
	}
	m_workerThreads.start(jobRunnable);
	return jobId;
}

bool GBAApp::removeWorkerJob(qint64 jobId) {
	for (auto& job : m_workerJobCallbacks.values(jobId)) {
		disconnect(job);
	}
	m_workerJobCallbacks.remove(jobId);
	if (!m_workerJobs.contains(jobId)) {
		return true;
	}
	bool success = false;
#if (QT_VERSION >= QT_VERSION_CHECK(5, 9, 0))
	success = m_workerThreads.tryTake(m_workerJobs[jobId]);
#endif
	if (success) {
		m_workerJobs.remove(jobId);
	}
	return success;
}

bool GBAApp::waitOnJob(qint64 jobId, QObject* context, std::function<void ()> callback) {
	if (!m_workerJobs.contains(jobId)) {
		return false;
	}
	if (!context) {
		context = this;
	}
	QMetaObject::Connection connection = connect(this, &GBAApp::jobFinished, context, [jobId, callback](qint64 testedJobId) {
		if (jobId != testedJobId) {
			return;
		}
		callback();
	});
	m_workerJobCallbacks.insert(m_nextJob, connection);
	return true;
}

void GBAApp::suspendScreensaver() {
#ifdef BUILD_SDL
	SDL::suspendScreensaver();
#endif
}

void GBAApp::resumeScreensaver() {
#ifdef BUILD_SDL
	SDL::resumeScreensaver();
#endif
}

void GBAApp::setScreensaverSuspendable(bool suspendable) {
	UNUSED(suspendable);
#ifdef BUILD_SDL
	SDL::setScreensaverSuspendable(suspendable);
#endif
}

void GBAApp::cleanupAfterUpdate() {
	// Remove leftover updater if there's one present
	QDir configDir(ConfigController::configDir());
	QString extractedPath = configDir.filePath(QLatin1String("updater"));
#ifdef Q_OS_WIN
	extractedPath += ".exe";
#endif
	QFile updater(extractedPath);
	if (updater.exists()) {
		updater.remove();
	}

#ifdef Q_OS_WIN
	// Remove the installer exe if we downloaded that too
	extractedPath = configDir.filePath(QLatin1String("update.exe"));
	QFile update(extractedPath);
	if (update.exists()) {
		update.remove();
	}
#endif
}

void GBAApp::restartForUpdate() {
	QFileInfo updaterPath(m_updater.updateInfo().url.path());
	QDir configDir(ConfigController::configDir());
	if (updaterPath.completeSuffix() == "exe") {
		m_invokeOnExit = configDir.filePath(QLatin1String("update.exe"));
	} else {
		QFile updater(":/updater");
		QString extractedPath = configDir.filePath(QLatin1String("updater"));
	#ifdef Q_OS_WIN
		extractedPath += ".exe";
	#endif
		updater.copy(extractedPath);
	#ifndef Q_OS_WIN
		QFile(extractedPath).setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
	#endif
		m_invokeOnExit = extractedPath;
	}

	for (auto& window : m_windows) {
		window->deleteLater();
	}
	QMetaObject::invokeMethod(this, "quit", Qt::QueuedConnection);
}

void GBAApp::finishJob(qint64 jobId) {
	m_workerJobs.remove(jobId);
	emit jobFinished(jobId);
	m_workerJobCallbacks.remove(jobId);
}

GBAApp::WorkerJob::WorkerJob(qint64 id, std::function<void ()> job, GBAApp* owner)
	: m_id(id)
	, m_job(job)
	, m_owner(owner)
{
	setAutoDelete(true);
}

void GBAApp::WorkerJob::run() {
	m_job();
	QMetaObject::invokeMethod(m_owner, "finishJob", Q_ARG(qint64, m_id));
}

#ifdef USE_SQLITE3
GameDBParser::GameDBParser(NoIntroDB* db, QObject* parent)
	: QObject(parent)
	, m_db(db)
{
	// Nothing to do
}

void GameDBParser::parseNoIntroDB() {
	VFile* vf = VFileDevice::open(GBAApp::dataDir() + "/nointro.dat", O_RDONLY);
	if (vf) {
		NoIntroDBLoadClrMamePro(m_db, vf);
		vf->close(vf);
	}
}

#endif
