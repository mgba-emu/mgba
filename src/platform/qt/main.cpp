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
#include <mgba/internal/gba/video.h>

#include <QLibraryInfo>
#include <QTranslator>

#ifdef QT_STATIC
#include <QtPlugin>
#ifdef _WIN32
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#ifdef BUILD_QT_MULTIMEDIA
Q_IMPORT_PLUGIN(QWindowsAudioPlugin);
#endif
#endif
#endif

using namespace QGBA;

int main(int argc, char* argv[]) {
#if defined(BUILD_SDL) && SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_SetMainReady();
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

	GBAApp application(argc, argv, &configController);

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
		w->resizeFrame(QSize(VIDEO_HORIZONTAL_PIXELS * graphicsOpts.multiplier, VIDEO_VERTICAL_PIXELS * graphicsOpts.multiplier));
	}
	if (graphicsOpts.fullscreen) {
		w->enterFullScreen();
	}

	w->show();

	return application.exec();
}
