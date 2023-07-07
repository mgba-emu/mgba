/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This must be defined before anything else is included.
#define SDL_MAIN_HANDLED

#include "ConfigController.h"
#include "GBAApp.h"
#include "Window.h"

#include <mgba/core/version.h>
#include <mgba/gba/interface.h>

#ifdef BUILD_SDL
#include "platform/sdl/sdl-events.h"
#endif

#include <QLibraryInfo>
#include <QTranslator>

#ifdef BUILD_GLES2
#include <QSurfaceFormat>
#endif

#ifdef QT_STATIC
#include <QtPlugin>
#ifdef Q_OS_WIN
Q_IMPORT_PLUGIN(QJpegPlugin);
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin);
#ifdef BUILD_QT_MULTIMEDIA
Q_IMPORT_PLUGIN(QWindowsAudioPlugin);
Q_IMPORT_PLUGIN(DSServicePlugin);
#endif
#elif defined(Q_OS_MAC)
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin);
#ifdef BUILD_QT_MULTIMEDIA
Q_IMPORT_PLUGIN(CoreAudioPlugin);
Q_IMPORT_PLUGIN(AVFServicePlugin);
#endif
#elif defined(Q_OS_UNIX)
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin);
Q_IMPORT_PLUGIN(QWaylandIntegrationPlugin);
#endif
#endif

#ifdef Q_OS_WIN
#include <process.h>
#include <wincon.h>
#else
#include <unistd.h>
#endif

using namespace QGBA;

int main(int argc, char* argv[]) {
#ifdef Q_OS_WIN
	AttachConsole(ATTACH_PARENT_PROCESS);
#endif
#ifdef BUILD_SDL
#if SDL_VERSION_ATLEAST(2, 0, 0) // CPP does not shortcut function lookup
	SDL_SetMainReady();
#endif
#endif

	ConfigController configController;

	QLocale locale;
	if (!configController.getQtOption("language").isNull()) {
		locale = QLocale(configController.getQtOption("language").toString());
		QLocale::setDefault(locale);
	}

	if (configController.parseArguments(argc, argv)) {
		if (configController.args()->showHelp) {
			configController.usage(argv[0]);
			return 0;
		}
		if (configController.args()->showVersion) {
			version(argv[0]);
			return 0;
		}
	} else {
		configController.usage(argv[0]);
		return 1;
	}

	QApplication::setApplicationName(projectName);
	QApplication::setApplicationVersion(projectVersion);
	QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);

#ifdef BUILD_GLES2
	QSurfaceFormat format;
	format.setVersion(3, 0);
	QSurfaceFormat::setDefaultFormat(format);
#endif

	GBAApp application(argc, argv, &configController);

#ifndef Q_OS_MAC
	QApplication::setWindowIcon(QIcon(":/res/mgba-256.png"));
#endif

	QTranslator qtTranslator;
	qtTranslator.load(locale, "qt", "_", QLibraryInfo::location(QLibraryInfo::TranslationsPath));
	application.installTranslator(&qtTranslator);

#ifdef QT_STATIC
	QTranslator qtStaticTranslator;
	qtStaticTranslator.load(locale, "qtbase", "_", ":/translations/");
	application.installTranslator(&qtStaticTranslator);
#endif

	QTranslator langTranslator;
	langTranslator.load(locale, binaryName, "-", ":/translations/");
	application.installTranslator(&langTranslator);

	Window* w = application.newWindow();
	w->loadConfig();
	w->argumentsPassed();

	w->show();

	int ret = application.exec();
	if (ret != 0) {
		return ret;
	}
	QString invoke = application.invokeOnExit();
	if (!invoke.isNull()) {
		QByteArray proc = invoke.toUtf8();
#ifdef Q_OS_WIN
		_execl(proc.constData(), proc.constData(), NULL);
#else
		execl(proc.constData(), proc.constData(), NULL);
#endif
	}

	return ret;
}

#ifdef _WIN32
#include <mgba-util/string.h>
#include <vector>

extern "C"
int wmain(int argc, wchar_t* argv[]) {
	std::vector<char*> argv8;
	for (int i = 0; i < argc; ++i) {
		argv8.push_back(utf16to8(reinterpret_cast<uint16_t*>(argv[i]), wcslen(argv[i]) * 2));
	}
	__argv = argv8.data();
	int ret = main(argc, argv8.data());
	for (char* ptr : argv8) {
		free(ptr);
	}
	return ret;
}
#endif
