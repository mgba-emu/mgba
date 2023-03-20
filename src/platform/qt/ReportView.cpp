/* Copyright (c) 2013-2020 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ReportView.h"

#include <QBuffer>
#include <QDesktopServices>
#include <QOffscreenSurface>
#include <QScreen>
#include <QSysInfo>
#include <QWindow>

#include <mgba/core/cheats.h>
#include <mgba/core/serialize.h>
#include <mgba/core/version.h>
#include <mgba-util/image/png-io.h>
#include <mgba-util/vfs.h>

#include "CoreController.h"
#include "GBAApp.h"
#include "Window.h"

#include "ui_ReportView.h"

#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
#define USE_CPUID
#include <cpuid.h>
#endif
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#define USE_CPUID
#endif

#if defined(BUILD_GL) || defined(BUILD_GLES2) || defined(BUILD_GLES3) || defined(USE_EPOXY)
#define DISPLAY_GL_INFO

#include "DisplayGL.h"

#include <QOpenGLFunctions>
#endif

#ifdef USE_EDITLINE
#include <histedit.h>
#endif

#ifdef USE_FFMPEG
#include <libavcodec/version.h>
#include <libavfilter/version.h>
#include <libavformat/version.h>
#include <libavutil/version.h>
#include <libswscale/version.h>
#ifdef USE_LIBAVRESAMPLE
#include <libavresample/version.h>
#endif
#ifdef USE_LIBSWRESAMPLE
#include <libswresample/version.h>
#endif
#endif

#ifdef USE_LIBZIP
#include <zip.h>
#endif

#ifdef USE_LUA
#include <lua.h>
#endif

#ifdef USE_LZMA
#include <7zVersion.h>
#endif

#ifdef BUILD_SDL
#include <SDL_version.h>
#endif

#ifdef USE_SQLITE3
#include "feature/sqlite3/no-intro.h"
#include <sqlite3.h>
#endif

#ifdef USE_ZLIB
#include <zlib.h>
#endif

using namespace QGBA;

static const QLatin1String yesNo[2] = {
	QLatin1String("No"),
	QLatin1String("Yes")
};

#ifdef USE_CPUID
unsigned ReportView::s_cpuidMax = 0xFFFFFFFF;
unsigned ReportView::s_cpuidExtMax = 0xFFFFFFFF;
#endif

ReportView::ReportView(QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
{
	m_ui.setupUi(this);

	QString description = m_ui.description->text();
	description.replace("{projectName}", QLatin1String(projectName));
	m_ui.description->setText(description);
	m_ui.fileView->setFont(GBAApp::app()->monospaceFont());

	connect(m_ui.fileList, &QListWidget::currentTextChanged, this, &ReportView::setShownReport);
}

void ReportView::generateReport() {
	m_displayOrder.clear();
	m_reports.clear();

	QDir configDir(ConfigController::configDir());

	QStringList swReport;
	swReport << QString("Name: %1").arg(QLatin1String(projectName));
	swReport << QString("Executable location: %1").arg(redact(QCoreApplication::applicationFilePath()));
	swReport << QString("Portable: %1").arg(yesNo[ConfigController::isPortable()]);
	swReport << QString("Configuration directory: %1").arg(redact(configDir.path()));
	swReport << QString("Version: %1").arg(QLatin1String(projectVersion));
	swReport << QString("Git branch: %1").arg(QLatin1String(gitBranch));
	swReport << QString("Git commit: %1").arg(QLatin1String(gitCommit));
	swReport << QString("Git revision: %1").arg(gitRevision);
	swReport << QString("OS: %1").arg(QSysInfo::prettyProductName());
	swReport << QString("Build architecture: %1").arg(QSysInfo::buildCpuArchitecture());
	swReport << QString("Run architecture: %1").arg(QSysInfo::currentCpuArchitecture());
	swReport << QString("Qt version: %1").arg(QLatin1String(qVersion()));
#ifdef USE_FFMPEG
	QStringList libavVers;
	libavVers << QLatin1String(LIBAVCODEC_IDENT);
	libavVers << QLatin1String(LIBAVFILTER_IDENT);
	libavVers << QLatin1String(LIBAVFORMAT_IDENT);
#ifdef USE_LIBAVRESAMPLE
	libavVers << QLatin1String(LIBAVRESAMPLE_IDENT);
#endif
	libavVers << QLatin1String(LIBAVUTIL_IDENT);
#ifdef USE_LIBSWRESAMPLE
	libavVers << QLatin1String(LIBSWRESAMPLE_IDENT);
#endif
	libavVers << QLatin1String(LIBSWSCALE_IDENT);
#ifdef USE_LIBAV
	swReport << QString("Libav versions: %1.%2").arg(libavVers.join(", "));
#else
	swReport << QString("FFmpeg versions: %1.%2").arg(libavVers.join(", "));
#endif
#else
	swReport << QString("FFmpeg not linked");
#endif
#ifdef USE_EDITLINE
	swReport << QString("libedit version: %1.%2").arg(LIBEDIT_MAJOR).arg(LIBEDIT_MINOR);
#else
	swReport << QString("libedit not linked");
#endif
#ifdef USE_ELF
	swReport << QString("libelf linked");
#else
	swReport << QString("libelf not linked");
#endif
#ifdef USE_PNG
	swReport << QString("libpng version: %1").arg(QLatin1String(PNG_LIBPNG_VER_STRING));
#else
	swReport << QString("libpng not linked");
#endif
#ifdef USE_LIBZIP
	swReport << QString("libzip version: %1").arg(QLatin1String(LIBZIP_VERSION));
#else
	swReport << QString("libzip not linked");
#endif
#ifdef USE_LZMA
	swReport << QString("libLZMA version: %1").arg(QLatin1String(MY_VERSION_NUMBERS));
#else
	swReport << QString("libLZMA not linked");
#endif
#ifdef USE_LUA
	swReport << QString("Lua version: %1").arg(QLatin1String(LUA_RELEASE));
#else
	swReport << QString("Lua not linked");
#endif
#ifdef USE_MINIZIP
	swReport << QString("minizip linked");
#else
	swReport << QString("minizip not linked");
#endif
#ifdef BUILD_SDL
	swReport << QString("SDL version: %1.%2.%3").arg(SDL_MAJOR_VERSION).arg(SDL_MINOR_VERSION).arg(SDL_PATCHLEVEL);
#else
	swReport << QString("SDL not linked");
#endif
#ifdef USE_SQLITE3
	swReport << QString("SQLite3 version: %1").arg(QLatin1String(SQLITE_VERSION));
#else
	swReport << QString("SQLite3 not linked");
#endif
#ifdef USE_ZLIB
	swReport << QString("zlib version: %1").arg(QLatin1String(ZLIB_VERSION));
#else
	swReport << QString("zlib not linked");
#endif
	addReport(QString("System info"), swReport.join('\n'));

	QStringList hwReport;
	addCpuInfo(hwReport);
	addGLInfo(hwReport);
	addReport(QString("Hardware info"), hwReport.join('\n'));

	QStringList controlsReport;
	addGamepadInfo(controlsReport);
	addReport(QString("Controls"), controlsReport.join('\n'));

	QList<QScreen*> screens = QGuiApplication::screens();
	std::sort(screens.begin(), screens.end(), [](const QScreen* a, const QScreen* b) {
		if (a->geometry().y() < b->geometry().y()) {
			return true;
		}
		if (a->geometry().x() < b->geometry().x()) {
			return true;
		}
		return false;
	});

	int screenId = 0;
	for (const QScreen* screen : screens) {
		++screenId;
		QStringList screenReport;
		addScreenInfo(screenReport, screen);
		addReport(QString("Screen %1").arg(screenId), screenReport.join('\n'));
	}

	QList<QPair<QString, QByteArray>> deferredBinaries;
	QList<ConfigController*> configs;
	int winId = 0;
	for (auto window : GBAApp::app()->windows()) {
		++winId;
		QStringList windowReport;
		auto controller = window->controller();
		ConfigController* config = window->config();
		if (configs.indexOf(config) < 0) {
			configs.append(config);
		}

		windowReport << QString("Window size: %1x%2").arg(window->width()).arg(window->height());
		windowReport << QString("Window location: %1, %2").arg(window->x()).arg(window->y());
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
		QScreen* screen = window->screen();
#else
		QScreen* screen = NULL;
		if (window->windowHandle()) {
			screen = window->windowHandle()->screen();
		}
#endif
		if (screen && screens.contains(screen)) {
			windowReport << QString("Screen: %1").arg(screens.contains(screen) + 1);
		} else {
			windowReport << QString("Screen: Unknown");
		}
		if (controller) {
			windowReport << QString("ROM open: Yes");

			{
				CoreController::Interrupter interrupter(controller);
				QFileInfo rom(window->windowFilePath());
				if (rom.exists()) {
					windowReport << QString("Filename: %1").arg(redact(rom.filePath()));
					windowReport << QString("Size: %1").arg(rom.size());
				}
				addROMInfo(windowReport, controller.get());

				mCheatDevice* device = controller->cheatDevice();
				if (device) {
					VFileDevice vf(VFileDevice::openMemory());
					mCheatSaveFile(device, vf);
					vf.seek(0);
					QByteArray cheats(vf.readAll());
					if (cheats.size()) {
						addReport(QString("Cheats %1").arg(winId), QString::fromUtf8(cheats));
					}
				}

				if (m_ui.includeSave->isChecked() && !m_ui.includeState->isChecked()) {
					// Only do the save separately if savestates aren't enabled, to guarantee consistency
					mCore* core = controller->thread()->core;
					void* sram = NULL;
					size_t size = core->savedataClone(core, &sram);
					if (sram) {
						QByteArray save(static_cast<const char*>(sram), size);
						free(sram);
						deferredBinaries.append(qMakePair(QString("Save %1").arg(winId), save));
					}
				}
			}
			if (m_ui.includeState->isChecked()) {
				QBuffer state;
				int flags = SAVESTATE_SCREENSHOT | SAVESTATE_CHEATS | SAVESTATE_RTC | SAVESTATE_METADATA;
				if (m_ui.includeSave->isChecked()) {
					flags |= SAVESTATE_SAVEDATA;
				}
				controller->saveState(&state, flags);
				deferredBinaries.append(qMakePair(QString("State %1").arg(winId), state.buffer()));
				if (m_ui.includeSave->isChecked()) {
					VFile* vf = VFileDevice::wrap(&state, QIODevice::ReadOnly);
					mStateExtdata extdata;
					mStateExtdataItem savedata;
					mStateExtdataInit(&extdata);
					if (mCoreExtractExtdata(controller->thread()->core, vf, &extdata) && mStateExtdataGet(&extdata, EXTDATA_SAVEDATA, &savedata)) {
						QByteArray save(static_cast<const char*>(savedata.data), savedata.size);
						deferredBinaries.append(qMakePair(QString("Save %1").arg(winId), save));
					}
					mStateExtdataDeinit(&extdata);
					vf->close(vf);
				}
			}
		} else {
			windowReport << QString("ROM open: No");
		}
		InputController* input = window->inputController();
		windowReport << QString("Active gamepad: %1").arg(input->gamepadIndex());
		windowReport << QString("Configuration: %1").arg(configs.indexOf(config) + 1);
		addReport(QString("Window %1").arg(winId), windowReport.join('\n'));
	}
	for (ConfigController* config : configs) {
		VFile* vf = VFileDevice::openMemory();
		mCoreConfigSaveVFile(config->config(), vf);
		void* contents = vf->map(vf, vf->size(vf), MAP_READ);
		if (contents) {
			QString report(QString::fromUtf8(static_cast<const char*>(contents), vf->size(vf)));
			addReport(QString("Configuration %1").arg(configs.indexOf(config) + 1), redact(report));
			vf->unmap(vf, contents, vf->size(vf));
		}
		vf->close(vf);
	}

	QFile qtIni(configDir.filePath("qt.ini"));
	if (qtIni.open(QIODevice::ReadOnly | QIODevice::Text)) {
		addReport(QString("Qt Configuration"), redact(QString::fromUtf8(qtIni.readAll())));
		qtIni.close();
	}

	std::sort(deferredBinaries.begin(), deferredBinaries.end());
	for (auto& pair : deferredBinaries) {
		addBinary(pair.first, pair.second);
	}

	rebuildModel();
}

void ReportView::save() {
#if defined(USE_LIBZIP) || defined(USE_MINIZIP)
	QString filename = GBAApp::app()->getSaveFileName(this, tr("Bug report archive"), tr("ZIP archive (*.zip)"));
	if (filename.isNull()) {
		return;
	}
	VDir* zip = VDirOpenZip(filename.toLocal8Bit().constData(), O_WRONLY | O_CREAT | O_TRUNC);
	if (!zip) {
		return;
	}
	for (const auto& filename : m_displayOrder) {
		VFileDevice vf(zip->openFile(zip, filename.toLocal8Bit().constData(), O_WRONLY));
		if (m_reports.contains(filename)) {
			vf.setTextModeEnabled(true);
			vf.write(m_reports[filename].toUtf8());
		} else if (m_binaries.contains(filename)) {
			vf.write(m_binaries[filename]);
		}
		vf.close();
	}
	zip->close(zip);
#endif
}

void ReportView::setShownReport(const QString& filename) {
	m_ui.fileView->setPlainText(m_reports[filename]);
}

void ReportView::rebuildModel() {
	m_ui.fileList->clear();
	for (const auto& filename : m_displayOrder) {
		QListWidgetItem* item = new QListWidgetItem(filename);
		if (m_binaries.contains(filename)) {
			item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
		}
		m_ui.fileList->addItem(item);
	}
#if defined(USE_LIBZIP) || defined(USE_MINIZIP)
	m_ui.save->setEnabled(true);
#endif
	m_ui.fileList->setEnabled(true);
	m_ui.fileView->setEnabled(true);
	m_ui.openList->setEnabled(true);
	m_ui.fileList->setCurrentRow(0);
	m_ui.fileView->installEventFilter(this);
}

void ReportView::openBugReportPage() {
	QDesktopServices::openUrl(QUrl("https://mgba.io/i/"));
}

void ReportView::addCpuInfo(QStringList& report) {
#ifdef USE_CPUID
	std::array<unsigned, 4> regs;
	if (!cpuid(0, regs.data())) {
		return;
	}
	unsigned vendor[4] = { regs[1], regs[3], regs[2], 0 };
	std::array<unsigned, 13> cpu{};
	cpuid(0x80000002, &cpu[0]);
	cpuid(0x80000003, &cpu[4]);
	cpuid(0x80000004, &cpu[8]);

	auto testBit = [](unsigned bit, unsigned reg) {
		return yesNo[bool(reg & (1 << bit))];
	};
	QStringList features;
	report << QString("CPU: %1").arg(QLatin1String(reinterpret_cast<char*>(cpu.data())));
	report << QString("CPU manufacturer: %1").arg(QLatin1String(reinterpret_cast<char*>(vendor)));
	cpuid(1, regs.data());
	unsigned family = ((regs[0] >> 8) & 0xF) | ((regs[0] >> 16) & 0xFF0);
	unsigned model = ((regs[0] >> 4) & 0xF) | ((regs[0] >> 12) & 0xF0);
	report << QString("CPU family ID: %1h").arg(family, 2, 16, QChar('0'));
	report << QString("CPU model ID: %1h").arg(model, 2, 16, QChar('0'));
	features << QString("Supports SSE: %1").arg(testBit(25, regs[3]));
	features << QString("Supports SSE2: %1").arg(testBit(26, regs[3]));
	features << QString("Supports SSE3: %1").arg(testBit(0, regs[2]));
	features << QString("Supports SSSE3: %1").arg(testBit(9, regs[2]));
	features << QString("Supports SSE4.1: %1").arg(testBit(19, regs[2]));
	features << QString("Supports SSE4.2: %1").arg(testBit(20, regs[2]));
	features << QString("Supports MOVBE: %1").arg(testBit(22, regs[2]));
	features << QString("Supports POPCNT: %1").arg(testBit(23, regs[2]));
	features << QString("Supports RDRAND: %1").arg(testBit(30, regs[2]));
	features << QString("Supports AVX: %1").arg(testBit(28, regs[2]));
	features << QString("Supports CMPXCHG8: %1").arg(testBit(8, regs[3]));
	features << QString("Supports CMPXCHG16: %1").arg(testBit(13, regs[2]));
	cpuid(7, 0, regs.data());
	features << QString("Supports AVX2: %1").arg(testBit(5, regs[1]));
	features << QString("Supports BMI1: %1").arg(testBit(3, regs[1]));
	features << QString("Supports BMI2: %1").arg(testBit(8, regs[1]));
	cpuid(0x80000001, regs.data());
	features << QString("Supports ABM: %1").arg(testBit(5, regs[2]));
	features << QString("Supports SSE4a: %1").arg(testBit(6, regs[2]));
	features.sort();
	report << features;
#endif
}

void ReportView::addGLInfo(QStringList& report) {
#ifdef DISPLAY_GL_INFO
	QSurfaceFormat format;

	report << QString("OpenGL type: %1").arg(QLatin1String(QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGL ? "OpenGL" : "OpenGL|ES"));

	format.setVersion(1, 4);
	report << QString("OpenGL supports legacy (1.x) contexts: %1").arg(yesNo[DisplayGL::supportsFormat(format)]);

	if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES) {
		format.setVersion(2, 0);
	} else {
		format.setVersion(3, 2);
	}
	format.setProfile(QSurfaceFormat::CoreProfile);
	report << QString("OpenGL supports core contexts: %1").arg(yesNo[DisplayGL::supportsFormat(format)]);

	QOpenGLContext context;
	if (context.create()) {
		QOffscreenSurface surface;
		surface.create();
		context.makeCurrent(&surface);
		report << QString("OpenGL renderer: %1").arg(QLatin1String(reinterpret_cast<const char*>(context.functions()->glGetString(GL_RENDERER))));
		report << QString("OpenGL vendor: %1").arg(QLatin1String(reinterpret_cast<const char*>(context.functions()->glGetString(GL_VENDOR))));
		report << QString("OpenGL version string: %1").arg(QLatin1String(reinterpret_cast<const char*>(context.functions()->glGetString(GL_VERSION))));
	}
#else
	report << QString("OpenGL support disabled at compilation time");
#endif
}

void ReportView::addGamepadInfo(QStringList& report) {
	InputController* input = GBAApp::app()->windows()[0]->inputController();
	QStringList gamepads = input->connectedGamepads();
	report << QString("Connected gamepads: %1").arg(gamepads.size());
	int i = 0;
	for (const auto& gamepad : gamepads) {
		report << QString("Gamepad %1: %2").arg(i).arg(gamepad);
		++i;
	}
	if (gamepads.size()) {
		i = 0;
		for (Window* window : GBAApp::app()->windows()) {
			++i;
			report << QString("Window %1 gamepad: %2").arg(i).arg(window->inputController()->gamepadIndex());
		}
	}
}

void ReportView::addROMInfo(QStringList& report, CoreController* controller) {
	report << QString("Currently paused: %1").arg(yesNo[controller->isPaused()]);

	mCore* core = controller->thread()->core;
	char title[17] = {};
	core->getGameTitle(core, title);
	report << QString("Internal title: %1").arg(QLatin1String(title));

	title[8] = '\0';
	core->getGameCode(core, title);
	if (title[0]) {
		report << QString("Game code: %1").arg(QLatin1String(title));
	} else {
		report << QString("Invalid game code");
	}

	uint32_t crc32 = 0;
	core->checksum(core, &crc32, mCHECKSUM_CRC32);
	report << QString("CRC32: %1").arg(crc32, 8, 16, QChar('0'));

#ifdef USE_SQLITE3
	const NoIntroDB* db = GBAApp::app()->gameDB();
	if (db && crc32) {
		NoIntroGame game{};
		if (NoIntroDBLookupGameByCRC(db, crc32, &game)) {
			report << QString("No-Intro name: %1").arg(game.name);
		} else {
			report << QString("Not present in No-Intro database").arg(game.name);
		}
	}
#endif
}

void ReportView::addScreenInfo(QStringList& report, const QScreen* screen) {
	QRect geometry = screen->geometry();

	report << QString("Size: %1x%2").arg(geometry.width()).arg(geometry.height());
	report << QString("Location: %1, %2").arg(geometry.x()).arg(geometry.y());
	report << QString("Refresh rate: %1 Hz").arg(screen->refreshRate());
#if (QT_VERSION >= QT_VERSION_CHECK(5, 5, 0))
	report << QString("Pixel ratio: %1").arg(screen->devicePixelRatio());
#endif
	report << QString("Logical DPI: %1x%2").arg(screen->logicalDotsPerInchX()).arg(screen->logicalDotsPerInchY());
	report << QString("Physical DPI: %1x%2").arg(screen->physicalDotsPerInchX()).arg(screen->physicalDotsPerInchY());
}

void ReportView::addReport(const QString& filename, const QString& report) {
	m_reports[filename] = report;
	m_displayOrder.append(filename);
}

void ReportView::addBinary(const QString& filename, const QByteArray& binary) {
	m_binaries[filename] = binary;
	m_displayOrder.append(filename);
}

QString ReportView::redact(const QString& text) {
	static QRegularExpression home(R"((?:\b|^)[A-Z]:[\\/](?:Users|Documents and Settings)[\\/][^\\/]+|(?:/usr)?/home/[^/]+)",
	                               QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
	QString redacted = text;
	redacted.replace(home, QString("[Home directory]"));
	return redacted;
}

bool ReportView::eventFilter(QObject*, QEvent* event) {
	if (event->type() != QEvent::FocusOut) {
		QListWidgetItem* currentReport = m_ui.fileList->currentItem();
		if (currentReport && !currentReport->text().isNull()) {
			m_reports[currentReport->text()] = m_ui.fileView->toPlainText();
		}
	}
	return false;
}

#ifdef USE_CPUID
bool ReportView::cpuid(unsigned id, unsigned* regs) {
	return cpuid(id, 0, regs);
}

bool ReportView::cpuid(unsigned id, unsigned sub, unsigned* regs) {
	if (s_cpuidMax == 0xFFFFFFFF) {
#ifdef _MSC_VER
		__cpuid(reinterpret_cast<int*>(regs), 0);
		s_cpuidMax = regs[0];
		__cpuid(reinterpret_cast<int*>(regs), 0x80000000);
		s_cpuidExtMax = regs[0];
#else
		s_cpuidMax = __get_cpuid_max(0, nullptr);
		s_cpuidExtMax = __get_cpuid_max(0x80000000, nullptr);
#endif
	}
	regs[0] = 0;
	regs[1] = 0;
	regs[2] = 0;
	regs[3] = 0;
	if (!(id & 0x80000000) && id > s_cpuidMax) {
		return false;
	}
	if ((id & 0x80000000) && id > s_cpuidExtMax) {
		return false;
	}

#ifdef _MSC_VER
	__cpuidex(reinterpret_cast<int*>(regs), id, sub);
#else
	__cpuid_count(id, sub, regs[0], regs[1], regs[2], regs[3]);
#endif
	return true;
}
#endif
