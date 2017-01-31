/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GBAApp.h"
#include "Window.h"

#include <QLibraryInfo>
#include <QTranslator>

#include <mgba/core/version.h>

#ifdef QT_STATIC
#include <QtPlugin>
#ifdef _WIN32
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#ifdef BUILD_QT_MULTIMEDIA
Q_IMPORT_PLUGIN(QWindowsAudioPlugin);
#endif
#endif
#endif

int main(int argc, char* argv[]) {
	QGBA::GBAApp application(argc, argv);

	QLocale locale = QLocale::system();

	QTranslator qtTranslator;
	qtTranslator.load(locale, "qt", "_", QLibraryInfo::location(QLibraryInfo::TranslationsPath));
	application.installTranslator(&qtTranslator);

	QTranslator langTranslator;
	langTranslator.load(locale, binaryName, "-", ":/translations/");
	application.installTranslator(&langTranslator);

	return application.exec();
}
