/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GBAApp.h"

#include "AudioProcessor.h"
#include "GameController.h"
#include "Window.h"

#include <QFileOpenEvent>

extern "C" {
#include "platform/commandline.h"
#include "util/socket.h"
}

using namespace QGBA;

GBAApp* g_app = nullptr;

GBAApp::GBAApp(int& argc, char* argv[])
	: QApplication(argc, argv)
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

Window* GBAApp::newWindow() {
	return g_app->newWindowInternal();
}
