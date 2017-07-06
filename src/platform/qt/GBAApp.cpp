/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GBAApp.h"

#include "AudioProcessor.h"
#include "ConfigController.h"
#include "Display.h"
#include "GameController.h"
#include "Window.h"
#include "VFileDevice.h"

#include <QFileInfo>
#include <QFileOpenEvent>
#include <QIcon>

#include <mgba/core/version.h>
#include <mgba-util/socket.h>
#include <mgba-util/vfs.h>

#ifdef USE_SQLITE3
#include "feature/sqlite3/no-intro.h"
#endif

using namespace QGBA;

static GBAApp* g_app = nullptr;

mLOG_DEFINE_CATEGORY(QT, "Qt", "platform.qt");

GBAApp::GBAApp(int& argc, char* argv[], ConfigController* config)
	: QApplication(argc, argv)
	, m_configController(config)
{
	g_app = this;

#ifdef BUILD_SDL
	SDL_Init(SDL_INIT_NOPARACHUTE);
#endif

#ifndef Q_OS_MAC
	setWindowIcon(QIcon(":/res/mgba-512.png"));
#endif

	SocketSubsystemInit();
	qRegisterMetaType<const uint32_t*>("const uint32_t*");
	qRegisterMetaType<mCoreThread*>("mCoreThread*");

	QApplication::setApplicationName(projectName);
	QApplication::setApplicationVersion(projectVersion);

	if (!m_configController->getQtOption("displayDriver").isNull()) {
		Display::setDriver(static_cast<Display::Driver>(m_configController->getQtOption("displayDriver").toInt()));
	}

	reloadGameDB();

	if (!m_configController->getQtOption("audioDriver").isNull()) {
		AudioProcessor::setDriver(static_cast<AudioProcessor::Driver>(m_configController->getQtOption("audioDriver").toInt()));
	}
}

GBAApp::~GBAApp() {
#ifdef USE_SQLITE3
	m_parseThread.quit();
	m_parseThread.wait();
#endif
}

bool GBAApp::event(QEvent* event) {
	if (event->type() == QEvent::FileOpen) {
		m_windows[0]->controller()->loadGame(static_cast<QFileOpenEvent*>(event)->file());
		return true;
	}
	return QApplication::event(event);
}

Window* GBAApp::newWindow() {
	if (m_windows.count() >= MAX_GBAS) {
		return nullptr;
	}
	Window* w = new Window(m_configController, m_multiplayer.attached());
	int windowId = m_multiplayer.attached();
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
	w->controller()->setMultiplayerController(&m_multiplayer);
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
		if (!window->controller()->isLoaded() || window->controller()->isPaused()) {
			continue;
		}
		window->controller()->setPaused(true);
		paused->append(window);
	}
}

void GBAApp::continueAll(const QList<Window*>& paused) {
	for (auto& window : paused) {
		window->controller()->setPaused(false);
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

QString GBAApp::getOpenDirectoryName(QWidget* owner, const QString& title) {
	QList<Window*> paused;
	pauseAll(&paused);
	QString filename = QFileDialog::getExistingDirectory(owner, title, m_configController->getOption("lastDirectory"));
	continueAll(paused);
	if (!filename.isEmpty()) {
		m_configController->setOption("lastDirectory", QFileInfo(filename).dir().canonicalPath());
	}
	return filename;
}

QString GBAApp::dataDir() {
#ifdef DATADIR
	QString path = QString::fromUtf8(DATADIR);
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
		if (m_parseThread.isRunning()) {
			m_parseThread.quit();
			m_parseThread.wait();
		}
		GameDBParser* parser = new GameDBParser(db);
		m_parseThread.start();
		parser->moveToThread(&m_parseThread);
		QMetaObject::invokeMethod(parser, "parseNoIntroDB");
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
