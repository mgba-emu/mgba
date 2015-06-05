/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GBAApp.h"

#include "AudioProcessor.h"
#include "GameController.h"
#include "Window.h"

#include <QFileInfo>
#include <QFileOpenEvent>

extern "C" {
#include "platform/commandline.h"
#include "util/socket.h"
}

using namespace QGBA;

static GBAApp* g_app = nullptr;

GBAApp::GBAApp(int& argc, char* argv[])
	: QApplication(argc, argv)
	, m_windows{}
{
	g_app = this;

#ifdef BUILD_SDL
	SDL_Init(SDL_INIT_NOPARACHUTE);
#endif

	SocketSubsystemInit();
	qRegisterMetaType<const uint32_t*>("const uint32_t*");

	QApplication::setApplicationName(projectName);
	QApplication::setApplicationVersion(projectVersion);

	Window* w = new Window(&m_configController);
	m_windows[0] = w;

#ifndef Q_OS_MAC
	w->show();
#endif

	GBAArguments args;
	if (m_configController.parseArguments(&args, argc, argv)) {
		w->argumentsPassed(&args);
	} else {
		w->loadConfig();
	}
	freeArguments(&args);

	AudioProcessor::setDriver(static_cast<AudioProcessor::Driver>(m_configController.getQtOption("audioDriver").toInt()));
	w->controller()->reloadAudioDriver();

	w->controller()->setMultiplayerController(&m_multiplayer);
#ifdef Q_OS_MAC
	w->show();
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
	if (m_multiplayer.attached() >= MAX_GBAS) {
		return nullptr;
	}
	Window* w = new Window(&m_configController, m_multiplayer.attached());
	m_windows[m_multiplayer.attached()] = w;
	w->setAttribute(Qt::WA_DeleteOnClose);
#ifndef Q_OS_MAC
	w->show();
#endif
	w->loadConfig();
	w->controller()->setMultiplayerController(&m_multiplayer);
#ifdef Q_OS_MAC
	w->show();
#endif
	return w;
}

GBAApp* GBAApp::app() {
	return g_app;
}

void GBAApp::interruptAll() {
	for (int i = 0; i < MAX_GBAS; ++i) {
		if (!m_windows[i] || !m_windows[i]->controller()->isLoaded()) {
			continue;
		}
		m_windows[i]->controller()->threadInterrupt();
	}
}

void GBAApp::continueAll() {
	for (int i = 0; i < MAX_GBAS; ++i) {
		if (!m_windows[i] || !m_windows[i]->controller()->isLoaded()) {
			continue;
		}
		m_windows[i]->controller()->threadContinue();
	}
}

QString GBAApp::getOpenFileName(QWidget* owner, const QString& title, const QString& filter) {
	interruptAll();
	QString filename = QFileDialog::getOpenFileName(owner, title, m_configController.getQtOption("lastDirectory").toString(), filter);
	continueAll();
	if (!filename.isEmpty()) {
		m_configController.setQtOption("lastDirectory", QFileInfo(filename).dir().path());
	}
	return filename;
}

QString GBAApp::getSaveFileName(QWidget* owner, const QString& title, const QString& filter) {
	interruptAll();
	QString filename = QFileDialog::getSaveFileName(owner, title, m_configController.getQtOption("lastDirectory").toString(), filter);
	continueAll();
	if (!filename.isEmpty()) {
		m_configController.setQtOption("lastDirectory", QFileInfo(filename).dir().path());
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

GBAApp::FileDialog::FileDialog(GBAApp* app, QWidget* parent, const QString& caption, const QString& filter)
	: QFileDialog(parent, caption, app->m_configController.getQtOption("lastDirectory").toString(), filter)
	, m_app(app)
{
}

int GBAApp::FileDialog::exec() {
	m_app->interruptAll();
	bool didAccept = QFileDialog::exec() == QDialog::Accepted;
	QStringList filenames = selectedFiles();
	if (!filenames.isEmpty()) {
		m_app->m_configController.setQtOption("lastDirectory", QFileInfo(filenames[0]).dir().path());
	}
	m_app->continueAll();
	return didAccept;
}
