/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GBAApp.h"

#include "AudioProcessor.h"
#include "Display.h"
#include "GameController.h"
#include "Window.h"
#include "VFileDevice.h"

#include <QFileInfo>
#include <QFileOpenEvent>
#include <QIcon>
#include <QTranslator>

#include "core/version.h"
#include "feature/commandline.h"
#include "util/nointro.h"
#include "util/socket.h"

using namespace QGBA;

static GBAApp* g_app = nullptr;

mLOG_DEFINE_CATEGORY(QT, "Qt");

GBAApp::GBAApp(int& argc, char* argv[])
	: QApplication(argc, argv)
	, m_windows{}
	, m_db(nullptr)
{
	g_app = this;

#ifdef BUILD_SDL
	SDL_Init(SDL_INIT_NOPARACHUTE);
#endif

#ifndef Q_OS_MAC
	setWindowIcon(QIcon(":/res/mgba-1024.png"));
#endif

	QTranslator* translator = new QTranslator(this);
	if (translator->load(QLocale(), QLatin1String(binaryName), QLatin1String("-"), QLatin1String(":/translations"))) {
		installTranslator(translator);
	}


	SocketSubsystemInit();
	qRegisterMetaType<const uint32_t*>("const uint32_t*");
	qRegisterMetaType<mCoreThread*>("mCoreThread*");

	QApplication::setApplicationName(projectName);
	QApplication::setApplicationVersion(projectVersion);

	if (!m_configController.getQtOption("displayDriver").isNull()) {
		Display::setDriver(static_cast<Display::Driver>(m_configController.getQtOption("displayDriver").toInt()));
	}

	mArguments args;
	mGraphicsOpts graphicsOpts;
	mSubParser subparser;
	initParserForGraphics(&subparser, &graphicsOpts);
	bool loaded = m_configController.parseArguments(&args, argc, argv, &subparser);
	if (loaded && args.showHelp) {
		usage(argv[0], subparser.usage);
		::exit(0);
		return;
	}

	reloadGameDB();

	if (!m_configController.getQtOption("audioDriver").isNull()) {
		AudioProcessor::setDriver(static_cast<AudioProcessor::Driver>(m_configController.getQtOption("audioDriver").toInt()));
	}
	Window* w = new Window(&m_configController);
	connect(w, &Window::destroyed, [this]() {
		m_windows[0] = nullptr;
	});
	m_windows[0] = w;

	if (loaded) {
		w->argumentsPassed(&args);
	} else {
		w->loadConfig();
	}
	freeArguments(&args);

	if (graphicsOpts.multiplier) {
		w->resizeFrame(QSize(VIDEO_HORIZONTAL_PIXELS * graphicsOpts.multiplier, VIDEO_VERTICAL_PIXELS * graphicsOpts.multiplier));
	}
	if (graphicsOpts.fullscreen) {
		w->enterFullScreen();
	}

	w->show();

	w->controller()->setMultiplayerController(&m_multiplayer);
	w->multiplayerChanged();
}

bool GBAApp::event(QEvent* event) {
	if (event->type() == QEvent::FileOpen) {
		m_windows[0]->controller()->loadGame(static_cast<QFileOpenEvent*>(event)->file());
		return true;
	}
	return QApplication::event(event);
}

Window* GBAApp::newWindow() {
	if (m_multiplayer.attached() >= MAX_GBAS) {
		return nullptr;
	}
	Window* w = new Window(&m_configController, m_multiplayer.attached());
	int windowId = m_multiplayer.attached();
	connect(w, &Window::destroyed, [this, windowId]() {
		m_windows[windowId] = nullptr;
	});
	m_windows[windowId] = w;
	w->setAttribute(Qt::WA_DeleteOnClose);
	w->loadConfig();
	w->show();
	w->controller()->setMultiplayerController(&m_multiplayer);
	w->multiplayerChanged();
	return w;
}

GBAApp* GBAApp::app() {
	return g_app;
}

void GBAApp::pauseAll(QList<int>* paused) {
	for (int i = 0; i < MAX_GBAS; ++i) {
		if (!m_windows[i] || !m_windows[i]->controller()->isLoaded() || m_windows[i]->controller()->isPaused()) {
			continue;
		}
		m_windows[i]->controller()->setPaused(true);
		paused->append(i);
	}
}

void GBAApp::continueAll(const QList<int>* paused) {
	for (int i : *paused) {
		m_windows[i]->controller()->setPaused(false);
	}
}

QString GBAApp::getOpenFileName(QWidget* owner, const QString& title, const QString& filter) {
	QList<int> paused;
	pauseAll(&paused);
	QString filename = QFileDialog::getOpenFileName(owner, title, m_configController.getOption("lastDirectory"), filter);
	continueAll(&paused);
	if (!filename.isEmpty()) {
		m_configController.setOption("lastDirectory", QFileInfo(filename).dir().path());
	}
	return filename;
}

QString GBAApp::getSaveFileName(QWidget* owner, const QString& title, const QString& filter) {
	QList<int> paused;
	pauseAll(&paused);
	QString filename = QFileDialog::getSaveFileName(owner, title, m_configController.getOption("lastDirectory"), filter);
	continueAll(&paused);
	if (!filename.isEmpty()) {
		m_configController.setOption("lastDirectory", QFileInfo(filename).dir().path());
	}
	return filename;
}

QString GBAApp::getOpenDirectoryName(QWidget* owner, const QString& title) {
	QList<int> paused;
	pauseAll(&paused);
	QString filename = QFileDialog::getExistingDirectory(owner, title, m_configController.getOption("lastDirectory"));
	continueAll(&paused);
	if (!filename.isEmpty()) {
		m_configController.setOption("lastDirectory", QFileInfo(filename).dir().path());
	}
	return filename;
}

QFileDialog* GBAApp::getOpenFileDialog(QWidget* owner, const QString& title, const QString& filter) {
	FileDialog* dialog = new FileDialog(this, owner, title, filter);
	dialog->setAcceptMode(QFileDialog::AcceptOpen);
	return dialog;
}

QFileDialog* GBAApp::getSaveFileDialog(QWidget* owner, const QString& title, const QString& filter) {
	FileDialog* dialog = new FileDialog(this, owner, title, filter);
	dialog->setAcceptMode(QFileDialog::AcceptSave);
	return dialog;
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

bool GBAApp::reloadGameDB() {
	NoIntroDB* db = nullptr;
	VFile* vf = VFileDevice::open(dataDir() + "/nointro.dat", O_RDONLY);
	if (vf) {
		db = NoIntroDBLoad(vf);
		vf->close(vf);
	}
	if (db && m_db) {
		NoIntroDBDestroy(m_db);
	}
	if (db) {
		m_db = db;
		return true;
	}
	return false;
}

GBAApp::FileDialog::FileDialog(GBAApp* app, QWidget* parent, const QString& caption, const QString& filter)
	: QFileDialog(parent, caption, app->m_configController.getOption("lastDirectory"), filter)
	, m_app(app)
{
}

int GBAApp::FileDialog::exec() {
	QList<int> paused;
	m_app->pauseAll(&paused);
	bool didAccept = QFileDialog::exec() == QDialog::Accepted;
	QStringList filenames = selectedFiles();
	if (!filenames.isEmpty()) {
		m_app->m_configController.setOption("lastDirectory", QFileInfo(filenames[0]).dir().path());
	}
	m_app->continueAll(&paused);
	return didAccept;
}
