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

#include <QLibraryInfo>
#include <QTranslator>

#ifdef BUILD_GLES2
#include <QSurfaceFormat>
#endif

#ifdef QT_STATIC
#include <QtPlugin>
#ifdef Q_OS_WIN
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
#endif
#endif

using namespace QGBA;

int main(int argc, char* argv[]) {
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

	mArguments args;
	mGraphicsOpts graphicsOpts;
	mSubParser subparser;
	initParserForGraphics(&subparser, &graphicsOpts);
	bool loaded = configController.parseArguments(&args, argc, argv, &subparser);
	if (loaded && args.showHelp) {
		usage(argv[0], subparser.usage);
		return 0;
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
	QApplication::setWindowIcon(QIcon(":/res/mgba-1024.png"));
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
	if (loaded) {
		w->argumentsPassed(&args);
	} else {
		w->loadConfig();
	}
	freeArguments(&args);

	if (graphicsOpts.multiplier) {
		w->resizeFrame(QSize(GBA_VIDEO_HORIZONTAL_PIXELS * graphicsOpts.multiplier, GBA_VIDEO_VERTICAL_PIXELS * graphicsOpts.multiplier));
	}
	if (graphicsOpts.fullscreen) {
		w->enterFullScreen();
	}

	w->show();

	return application.exec();
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
	int ret = main(argc, argv8.data());
	for (char* ptr : argv8) {
		free(ptr);
	}
	return ret;
}
#endif
