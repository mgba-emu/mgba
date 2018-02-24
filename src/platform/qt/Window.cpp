/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "Window.h"

#include <QDesktopWidget>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QStackedLayout>

#ifdef USE_SQLITE3
#include "ArchiveInspector.h"
#include "library/LibraryController.h"
#endif

#include "AboutScreen.h"
#include "CheatsView.h"
#include "ConfigController.h"
#include "DebuggerConsole.h"
#include "DebuggerConsoleController.h"
#include "Display.h"
#include "GameController.h"
#include "GBAApp.h"
#include "GDBController.h"
#include "GDBWindow.h"
#include "GIFView.h"
#include "IOViewer.h"
#include "LoadSaveState.h"
#include "LogView.h"
#include "MultiplayerController.h"
#include "MemorySearch.h"
#include "MemoryView.h"
#include "OverrideView.h"
#include "ObjView.h"
#include "PaletteView.h"
#include "ROMInfo.h"
#include "SensorView.h"
#include "SettingsView.h"
#include "ShaderSelector.h"
#include "ShortcutController.h"
#include "TileView.h"
#include "VideoView.h"

#include <mgba/core/version.h>
#ifdef M_CORE_GB
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/video.h>
#endif
#ifdef M_CORE_GBA
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/video.h>
#endif
#include <mgba/feature/commandline.h>
#include "feature/sqlite3/no-intro.h"
#include <mgba-util/vfs.h>

using namespace QGBA;

Window::Window(ConfigController* config, int playerId, QWidget* parent)
	: QMainWindow(parent)
	, m_logView(new LogView(&m_log))
	, m_screenWidget(new WindowBackground())
	, m_config(config)
	, m_inputController(playerId, this)
	, m_shortcutController(new ShortcutController(this))
{
	setFocusPolicy(Qt::StrongFocus);
	setAcceptDrops(true);
	setAttribute(Qt::WA_DeleteOnClose);
	m_controller = new GameController(this);
	m_controller->setInputController(&m_inputController);
	updateTitle();

	m_display = Display::create(this);
#if defined(BUILD_GL) || defined(BUILD_GLES2)
	m_shaderView = new ShaderSelector(m_display, m_config);
#endif

	m_logo.setDevicePixelRatio(m_screenWidget->devicePixelRatio());
	m_logo = m_logo; // Free memory left over in old pixmap

	m_screenWidget->setMinimumSize(m_display->minimumSize());
	m_screenWidget->setSizePolicy(m_display->sizePolicy());
#if defined(M_CORE_GBA)
	float i = 2;
#elif defined(M_CORE_GB)
	float i = 3;
#endif
	QVariant multiplier = m_config->getOption("scaleMultiplier");
	if (!multiplier.isNull()) {
		m_savedScale = multiplier.toInt();
		i = m_savedScale;
	}
#ifdef USE_SQLITE3
	m_libraryView = new LibraryController(nullptr, ConfigController::configDir() + "/library.sqlite3", m_config);
	ConfigOption* showLibrary = m_config->addOption("showLibrary");
	showLibrary->connect([this](const QVariant& value) {
		if (value.toBool()) {
			if (m_controller->isLoaded()) {
				m_screenWidget->layout()->addWidget(m_libraryView);
			} else {
				attachWidget(m_libraryView);
			}
		} else {
			detachWidget(m_libraryView);
		}
	}, this);
	m_config->updateOption("showLibrary");
	ConfigOption* libraryStyle = m_config->addOption("libraryStyle");
	libraryStyle->connect([this](const QVariant& value) {
		m_libraryView->setViewStyle(static_cast<LibraryStyle>(value.toInt()));
	}, this);
	m_config->updateOption("libraryStyle");

	connect(m_libraryView, &LibraryController::startGame, [this]() {
		VFile* output = m_libraryView->selectedVFile();
		if (output) {
			QPair<QString, QString> path = m_libraryView->selectedPath();
			m_controller->loadGame(output, path.second, path.first);
		}
	});
#endif
#if defined(M_CORE_GBA)
	resizeFrame(QSize(VIDEO_HORIZONTAL_PIXELS * i, VIDEO_VERTICAL_PIXELS * i));
#elif defined(M_CORE_GB)
	resizeFrame(QSize(GB_VIDEO_HORIZONTAL_PIXELS * i, GB_VIDEO_VERTICAL_PIXELS * i));
#endif
	m_screenWidget->setPixmap(m_logo);
	m_screenWidget->setDimensions(m_logo.width(), m_logo.height());
	m_screenWidget->setLockIntegerScaling(false);
	m_screenWidget->setLockAspectRatio(true);
	setCentralWidget(m_screenWidget);

	connect(m_controller, &GameController::gameStarted, this, &Window::gameStarted);
	connect(m_controller, &GameController::gameStarted, &m_inputController, &InputController::suspendScreensaver);
	connect(m_controller, &GameController::gameStopped, m_display, &Display::stopDrawing);
	connect(m_controller, &GameController::gameStopped, this, &Window::gameStopped);
	connect(m_controller, &GameController::gameStopped, &m_inputController, &InputController::resumeScreensaver);
	connect(m_controller, &GameController::stateLoaded, m_display, &Display::forceDraw);
	connect(m_controller, &GameController::rewound, m_display, &Display::forceDraw);
	connect(m_controller, &GameController::gamePaused, [this](mCoreThread* context) {
		unsigned width, height;
		context->core->desiredVideoDimensions(context->core, &width, &height);
		QImage currentImage(reinterpret_cast<const uchar*>(m_controller->drawContext()), width, height,
		                    width * BYTES_PER_PIXEL, QImage::Format_RGBX8888);
		QPixmap pixmap;
		pixmap.convertFromImage(currentImage);
		m_screenWidget->setPixmap(pixmap);
	});
	connect(m_controller, &GameController::gamePaused, m_display, &Display::pauseDrawing);
#ifndef Q_OS_MAC
	connect(m_controller, &GameController::gamePaused, menuBar(), &QWidget::show);
	connect(m_controller, &GameController::gameUnpaused, [this]() {
		if(isFullScreen()) {
			menuBar()->hide();
		}
	});
#endif
	connect(m_controller, &GameController::gamePaused, &m_inputController, &InputController::resumeScreensaver);
	connect(m_controller, &GameController::gameUnpaused, m_display, &Display::unpauseDrawing);
	connect(m_controller, &GameController::gameUnpaused, &m_inputController, &InputController::suspendScreensaver);
	connect(m_controller, &GameController::postLog, &m_log, &LogController::postLog);
	connect(m_controller, &GameController::frameAvailable, this, &Window::recordFrame);
	connect(m_controller, &GameController::frameAvailable, m_display, &Display::framePosted);
	connect(m_controller, &GameController::gameCrashed, this, &Window::gameCrashed);
	connect(m_controller, &GameController::gameFailed, this, &Window::gameFailed);
	connect(m_controller, &GameController::unimplementedBiosCall, this, &Window::unimplementedBiosCall);
	connect(m_controller, &GameController::statusPosted, m_display, &Display::showMessage);
	connect(&m_log, &LogController::levelsSet, m_controller, &GameController::setLogLevel);
	connect(&m_log, &LogController::levelsEnabled, m_controller, &GameController::enableLogLevel);
	connect(&m_log, &LogController::levelsDisabled, m_controller, &GameController::disableLogLevel);
	connect(this, &Window::startDrawing, m_display, &Display::startDrawing, Qt::QueuedConnection);
	connect(this, &Window::shutdown, m_display, &Display::stopDrawing);
	connect(this, &Window::shutdown, m_controller, &GameController::closeGame);
	connect(this, &Window::shutdown, m_logView, &QWidget::hide);
	connect(this, &Window::audioBufferSamplesChanged, m_controller, &GameController::setAudioBufferSamples);
	connect(this, &Window::sampleRateChanged, m_controller, &GameController::setAudioSampleRate);
	connect(this, &Window::fpsTargetChanged, m_controller, &GameController::setFPSTarget);
	connect(&m_fpsTimer, &QTimer::timeout, this, &Window::showFPS);
	connect(&m_focusCheck, &QTimer::timeout, this, &Window::focusCheck);
	connect(m_display, &Display::hideCursor, [this]() {
		if (static_cast<QStackedLayout*>(m_screenWidget->layout())->currentWidget() == m_display) {
			m_screenWidget->setCursor(Qt::BlankCursor);
		}
	});
	connect(m_display, &Display::showCursor, [this]() {
		m_screenWidget->unsetCursor();
	});
	connect(&m_inputController, &InputController::profileLoaded, m_shortcutController, &ShortcutController::loadProfile);

	m_log.setLevels(mLOG_WARN | mLOG_ERROR | mLOG_FATAL);
	m_fpsTimer.setInterval(FPS_TIMER_INTERVAL);
	m_focusCheck.setInterval(200);

	m_shortcutController->setConfigController(m_config);
	setupMenu(menuBar());
}

Window::~Window() {
	delete m_logView;

#ifdef USE_FFMPEG
	delete m_videoView;
#endif

#ifdef USE_MAGICK
	delete m_gifView;
#endif

#ifdef USE_SQLITE3
	delete m_libraryView;
#endif
}

void Window::argumentsPassed(mArguments* args) {
	loadConfig();

	if (args->patch) {
		m_controller->loadPatch(args->patch);
	}

	if (args->fname) {
		m_controller->loadGame(args->fname);
	}

#ifdef USE_GDB_STUB
	if (args->debuggerType == DEBUGGER_GDB) {
		if (!m_gdbController) {
			m_gdbController = new GDBController(m_controller, this);
			m_gdbController->listen();
		}
	}
#endif
}

void Window::resizeFrame(const QSize& size) {
	QSize newSize(size);
	m_screenWidget->setSizeHint(newSize);
	newSize -= m_screenWidget->size();
	newSize += this->size();
	if (!isFullScreen()) {
		resize(newSize);
	}
}

void Window::setConfig(ConfigController* config) {
	m_config = config;
}

void Window::loadConfig() {
	const mCoreOptions* opts = m_config->options();
	reloadConfig();

	// TODO: Move these to ConfigController
	if (opts->fpsTarget) {
		emit fpsTargetChanged(opts->fpsTarget);
	}

	if (opts->audioBuffers) {
		emit audioBufferSamplesChanged(opts->audioBuffers);
	}

	if (opts->sampleRate) {
		emit sampleRateChanged(opts->sampleRate);
	}

	if (opts->width && opts->height) {
		resizeFrame(QSize(opts->width, opts->height));
	}

	if (opts->fullscreen) {
		enterFullScreen();
	}

#if defined(BUILD_GL) || defined(BUILD_GLES2)
	if (opts->shader) {
		struct VDir* shader = VDirOpen(opts->shader);
		if (shader) {
			m_display->setShaders(shader);
			m_shaderView->refreshShaders();
			shader->close(shader);
		}
	}
#endif

	m_mruFiles = m_config->getMRU();
	updateMRU();

	m_inputController.setConfiguration(m_config);
	m_controller->setUseBIOS(opts->useBios);
}

void Window::reloadConfig() {
	const mCoreOptions* opts = m_config->options();

	m_log.setLevels(opts->logLevel);

	m_controller->setConfig(m_config->config());
	m_display->lockAspectRatio(opts->lockAspectRatio);
	m_display->filter(opts->resampleVideo);

	m_inputController.setScreensaverSuspendable(opts->suspendScreensaver);
}

void Window::saveConfig() {
	m_inputController.saveConfiguration();
	m_config->write();
}

QString Window::getFilters() const {
	QStringList filters;
	QStringList formats;

#ifdef M_CORE_GBA
	QStringList gbaFormats{
		"*.gba",
#if defined(USE_LIBZIP) || defined(USE_ZLIB)
		"*.zip",
#endif
#ifdef USE_LZMA
		"*.7z",
#endif
		"*.agb",
		"*.mb",
		"*.rom",
		"*.bin"};
	formats.append(gbaFormats);
	filters.append(tr("Game Boy Advance ROMs (%1)").arg(gbaFormats.join(QChar(' '))));
#endif

#ifdef M_CORE_GB
	QStringList gbFormats{
		"*.gb",
		"*.gbc",
#if defined(USE_LIBZIP) || defined(USE_ZLIB)
		"*.zip",
#endif
#ifdef USE_LZMA
		"*.7z",
#endif
		"*.rom",
		"*.bin"};
	formats.append(gbFormats);
	filters.append(tr("Game Boy ROMs (%1)").arg(gbFormats.join(QChar(' '))));
#endif

	formats.removeDuplicates();
	filters.prepend(tr("All ROMs (%1)").arg(formats.join(QChar(' '))));
	filters.append(tr("%1 Video Logs (*.mvl)").arg(projectName));
	return filters.join(";;");
}

QString Window::getFiltersArchive() const {
	QStringList filters;

	QStringList formats{
#if defined(USE_LIBZIP) || defined(USE_ZLIB)
		"*.zip",
#endif
#ifdef USE_LZMA
		"*.7z",
#endif
	};
	filters.append(tr("Archives (%1)").arg(formats.join(QChar(' '))));
	return filters.join(";;");
}

void Window::selectROM() {
	QString filename = GBAApp::app()->getOpenFileName(this, tr("Select ROM"), getFilters());
	if (!filename.isEmpty()) {
		m_controller->loadGame(filename);
	}
}

#ifdef USE_SQLITE3
void Window::selectROMInArchive() {
	QString filename = GBAApp::app()->getOpenFileName(this, tr("Select ROM"), getFiltersArchive());
	if (filename.isEmpty()) {
		return;
	}
	ArchiveInspector* archiveInspector = new ArchiveInspector(filename);
	connect(archiveInspector, &QDialog::accepted, [this,  archiveInspector]() {
		VFile* output = archiveInspector->selectedVFile();
		QPair<QString, QString> path = archiveInspector->selectedPath();
		if (output) {
			m_controller->loadGame(output, path.second, path.first);
		}
		archiveInspector->close();
	});
	archiveInspector->setAttribute(Qt::WA_DeleteOnClose);
	archiveInspector->show();
}

void Window::addDirToLibrary() {
	QString filename = GBAApp::app()->getOpenDirectoryName(this, tr("Select folder"));
	if (filename.isEmpty()) {
		return;
	}
	m_libraryView->addDirectory(filename);
}
#endif

void Window::replaceROM() {
	QString filename = GBAApp::app()->getOpenFileName(this, tr("Select ROM"), getFilters());
	if (!filename.isEmpty()) {
		m_controller->replaceGame(filename);
	}
}

void Window::selectSave(bool temporary) {
	QStringList formats{"*.sav"};
	QString filter = tr("Game Boy Advance save files (%1)").arg(formats.join(QChar(' ')));
	QString filename = GBAApp::app()->getOpenFileName(this, tr("Select save"), filter);
	if (!filename.isEmpty()) {
		m_controller->loadSave(filename, temporary);
	}
}

void Window::multiplayerChanged() {
	int attached = 1;
	MultiplayerController* multiplayer = m_controller->multiplayerController();
	if (multiplayer) {
		attached = multiplayer->attached();
	}
	if (m_controller->isLoaded()) {
		for (QAction* action : m_nonMpActions) {
			action->setDisabled(attached > 1);
		}
	}
}

void Window::selectPatch() {
	QString filename = GBAApp::app()->getOpenFileName(this, tr("Select patch"), tr("Patches (*.ips *.ups *.bps)"));
	if (!filename.isEmpty()) {
		m_controller->loadPatch(filename);
	}
}

void Window::openView(QWidget* widget) {
	connect(this, &Window::shutdown, widget, &QWidget::close);
	connect(m_controller, &GameController::gameStopped, widget, &QWidget::close);
	widget->setAttribute(Qt::WA_DeleteOnClose);
	widget->show();
}

void Window::importSharkport() {
	QString filename = GBAApp::app()->getOpenFileName(this, tr("Select save"), tr("GameShark saves (*.sps *.xps)"));
	if (!filename.isEmpty()) {
		m_controller->importSharkport(filename);
	}
}

void Window::exportSharkport() {
	QString filename = GBAApp::app()->getSaveFileName(this, tr("Select save"), tr("GameShark saves (*.sps *.xps)"));
	if (!filename.isEmpty()) {
		m_controller->exportSharkport(filename);
	}
}

void Window::openSettingsWindow() {
	SettingsView* settingsWindow = new SettingsView(m_config, &m_inputController, m_shortcutController);
#if defined(BUILD_GL) || defined(BUILD_GLES2)
	if (m_display->supportsShaders()) {
		settingsWindow->setShaderSelector(m_shaderView);
	}
#endif
	connect(settingsWindow, &SettingsView::biosLoaded, m_controller, &GameController::loadBIOS);
	connect(settingsWindow, &SettingsView::audioDriverChanged, m_controller, &GameController::reloadAudioDriver);
	connect(settingsWindow, &SettingsView::displayDriverChanged, this, &Window::mustRestart);
	connect(settingsWindow, &SettingsView::languageChanged, this, &Window::mustRestart);
	connect(settingsWindow, &SettingsView::pathsChanged, this, &Window::reloadConfig);
	connect(settingsWindow, &SettingsView::libraryCleared, m_libraryView, &LibraryController::clear);
	openView(settingsWindow);
}

void Window::openAboutScreen() {
	AboutScreen* about = new AboutScreen();
	openView(about);
}

void Window::startVideoLog() {
	QString filename = GBAApp::app()->getSaveFileName(this, tr("Select video log"), tr("Video logs (*.mvl)"));
	if (!filename.isEmpty()) {
		m_controller->startVideoLog(filename);
	}
}

template <typename T, typename A>
std::function<void()> Window::openTView(A arg) {
	return [=]() {
		T* view = new T(m_controller, arg);
		openView(view);
	};
}

template <typename T>
std::function<void()> Window::openTView() {
	return [=]() {
		T* view = new T(m_controller);
		openView(view);
	};
}

#ifdef USE_FFMPEG
void Window::openVideoWindow() {
	if (!m_videoView) {
		m_videoView = new VideoView();
		connect(m_videoView, &VideoView::recordingStarted, m_controller, &GameController::setAVStream);
		connect(m_videoView, &VideoView::recordingStopped, m_controller, &GameController::clearAVStream, Qt::DirectConnection);
		connect(m_controller, &GameController::gameStopped, m_videoView, &VideoView::stopRecording);
		connect(m_controller, &GameController::gameStopped, m_videoView, &QWidget::close);
		connect(m_controller, &GameController::gameStarted, [this]() {
			m_videoView->setNativeResolution(m_controller->screenDimensions());
		});
		if (m_controller->isLoaded()) {
			m_videoView->setNativeResolution(m_controller->screenDimensions());
		}
		connect(this, &Window::shutdown, m_videoView, &QWidget::close);
	}
	m_videoView->show();
}
#endif

#ifdef USE_MAGICK
void Window::openGIFWindow() {
	if (!m_gifView) {
		m_gifView = new GIFView();
		connect(m_gifView, &GIFView::recordingStarted, m_controller, &GameController::setAVStream);
		connect(m_gifView, &GIFView::recordingStopped, m_controller, &GameController::clearAVStream, Qt::DirectConnection);
		connect(m_controller, &GameController::gameStopped, m_gifView, &GIFView::stopRecording);
		connect(m_controller, &GameController::gameStopped, m_gifView, &QWidget::close);
		connect(this, &Window::shutdown, m_gifView, &QWidget::close);
	}
	m_gifView->show();
}
#endif

#ifdef USE_GDB_STUB
void Window::gdbOpen() {
	if (!m_gdbController) {
		m_gdbController = new GDBController(m_controller, this);
	}
	GDBWindow* window = new GDBWindow(m_gdbController);
	openView(window);
}
#endif

#ifdef USE_DEBUGGERS
void Window::consoleOpen() {
	if (!m_console) {
		m_console = new DebuggerConsoleController(m_controller, this);
	}
	DebuggerConsole* window = new DebuggerConsole(m_console);
	openView(window);
}
#endif

void Window::keyPressEvent(QKeyEvent* event) {
	if (event->isAutoRepeat()) {
		QWidget::keyPressEvent(event);
		return;
	}
	GBAKey key = m_inputController.mapKeyboard(event->key());
	if (key == GBA_KEY_NONE) {
		QWidget::keyPressEvent(event);
		return;
	}
	m_controller->keyPressed(key);
	event->accept();
}

void Window::keyReleaseEvent(QKeyEvent* event) {
	if (event->isAutoRepeat()) {
		QWidget::keyReleaseEvent(event);
		return;
	}
	GBAKey key = m_inputController.mapKeyboard(event->key());
	if (key == GBA_KEY_NONE) {
		QWidget::keyPressEvent(event);
		return;
	}
	m_controller->keyReleased(key);
	event->accept();
}

void Window::resizeEvent(QResizeEvent* event) {
	if (!isFullScreen()) {
		m_config->setOption("height", m_screenWidget->height());
		m_config->setOption("width", m_screenWidget->width());
	}

	int factor = 0;
	QSize size(VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
	if (m_controller->isLoaded()) {
		size = m_controller->screenDimensions();
	}
	if (m_screenWidget->width() % size.width() == 0 && m_screenWidget->height() % size.height() == 0 &&
	    m_screenWidget->width() / size.width() == m_screenWidget->height() / size.height()) {
		factor = m_screenWidget->width() / size.width();
	} else {
		m_savedScale = 0;
	}
	for (QMap<int, QAction*>::iterator iter = m_frameSizes.begin(); iter != m_frameSizes.end(); ++iter) {
		bool enableSignals = iter.value()->blockSignals(true);
		iter.value()->setChecked(iter.key() == factor);
		iter.value()->blockSignals(enableSignals);
	}

	m_config->setOption("fullscreen", isFullScreen());
}

void Window::showEvent(QShowEvent* event) {
	if (m_wasOpened) {
		return;
	}
	m_wasOpened = true;
	resizeFrame(m_screenWidget->sizeHint());
	QVariant windowPos = m_config->getQtOption("windowPos");
	if (!windowPos.isNull()) {
		move(windowPos.toPoint());
	} else {
		QRect rect = frameGeometry();
		rect.moveCenter(QApplication::desktop()->availableGeometry().center());
		move(rect.topLeft());
	}
	if (m_fullscreenOnStart) {
		enterFullScreen();
		m_fullscreenOnStart = false;
	}
}

void Window::closeEvent(QCloseEvent* event) {
	emit shutdown();
	m_config->setQtOption("windowPos", pos());

	if (m_savedScale > 0) {
		m_config->setOption("height", VIDEO_VERTICAL_PIXELS * m_savedScale);
		m_config->setOption("width", VIDEO_HORIZONTAL_PIXELS * m_savedScale);
	}
	saveConfig();
	QMainWindow::closeEvent(event);
}

void Window::focusInEvent(QFocusEvent*) {
	m_display->forceDraw();
}

void Window::focusOutEvent(QFocusEvent*) {
	m_controller->setTurbo(false, false);
	m_controller->stopRewinding();
	m_controller->clearKeys();
}

void Window::dragEnterEvent(QDragEnterEvent* event) {
	if (event->mimeData()->hasFormat("text/uri-list")) {
		event->acceptProposedAction();
	}
}

void Window::dropEvent(QDropEvent* event) {
	QString uris = event->mimeData()->data("text/uri-list");
	uris = uris.trimmed();
	if (uris.contains("\n")) {
		// Only one file please
		return;
	}
	QUrl url(uris);
	if (!url.isLocalFile()) {
		// No remote loading
		return;
	}
	event->accept();
	m_controller->loadGame(url.toLocalFile());
}

void Window::mouseDoubleClickEvent(QMouseEvent* event) {
	if (event->button() != Qt::LeftButton) {
		return;
	}
	toggleFullScreen();
}

void Window::enterFullScreen() {
	if (!isVisible()) {
		m_fullscreenOnStart = true;
		return;
	}
	if (isFullScreen()) {
		return;
	}
	showFullScreen();
#ifndef Q_OS_MAC
	if (m_controller->isLoaded() && !m_controller->isPaused()) {
		menuBar()->hide();
	}
#endif
}

void Window::exitFullScreen() {
	if (!isFullScreen()) {
		return;
	}
	m_screenWidget->unsetCursor();
	menuBar()->show();
	showNormal();
}

void Window::toggleFullScreen() {
	if (isFullScreen()) {
		exitFullScreen();
	} else {
		enterFullScreen();
	}
}

void Window::gameStarted(mCoreThread* context, const QString& fname) {
	MutexLock(&context->stateMutex);
	if (context->state < THREAD_EXITING) {
		emit startDrawing(context);
	} else {
		MutexUnlock(&context->stateMutex);
		return;
	}
	MutexUnlock(&context->stateMutex);
	for (QAction* action : m_gameActions) {
		action->setDisabled(false);
	}
#ifdef M_CORE_GBA
	for (QAction* action : m_gbaActions) {
		action->setDisabled(context->core->platform(context->core) != PLATFORM_GBA);
	}
#endif
	multiplayerChanged();
	if (!fname.isEmpty()) {
		setWindowFilePath(fname);
		appendMRU(fname);
	}
	updateTitle();
	unsigned width, height;
	context->core->desiredVideoDimensions(context->core, &width, &height);
	m_display->setMinimumSize(width, height);
	m_screenWidget->setMinimumSize(m_display->minimumSize());
	m_screenWidget->setDimensions(width, height);
	m_config->updateOption("lockIntegerScaling");
	m_config->updateOption("lockAspectRatio");
	if (m_savedScale > 0) {
		resizeFrame(QSize(width, height) * m_savedScale);
	}
	attachWidget(m_display);

#ifndef Q_OS_MAC
	if (isFullScreen()) {
		menuBar()->hide();
	}
#endif

	m_hitUnimplementedBiosCall = false;
	m_fpsTimer.start();
	m_focusCheck.start();
	if (m_display->underMouse()) {
		m_screenWidget->setCursor(Qt::BlankCursor);
	}

	m_controller->threadInterrupt();
	if (m_controller->isLoaded()) {
		mCore* core = m_controller->thread()->core;
		const mCoreChannelInfo* videoLayers;
		const mCoreChannelInfo* audioChannels;
		size_t nVideo = core->listVideoLayers(core, &videoLayers);
		size_t nAudio = core->listAudioChannels(core, &audioChannels);

		if (nVideo) {
			for (size_t i = 0; i < nVideo; ++i) {
				QAction* action = new QAction(videoLayers[i].visibleName, m_videoLayers);
				action->setCheckable(true);
				action->setChecked(true);
				connect(action, &QAction::triggered, [this, videoLayers, i](bool enable) {
					m_controller->setVideoLayerEnabled(videoLayers[i].id, enable);
				});
				m_videoLayers->addAction(action);
			}
		}
		if (nAudio) {
			for (size_t i = 0; i < nAudio; ++i) {
				QAction* action = new QAction(audioChannels[i].visibleName, m_audioChannels);
				action->setCheckable(true);
				action->setChecked(true);
				connect(action, &QAction::triggered, [this, audioChannels, i](bool enable) {
					m_controller->setAudioChannelEnabled(audioChannels[i].id, enable);
				});
				m_audioChannels->addAction(action);
			}
		}
	}
	m_controller->threadContinue();
}

void Window::gameStopped() {
#ifdef M_CORE_GBA
	for (QAction* action : m_gbaActions) {
		action->setDisabled(false);
	}
#endif
	for (QAction* action : m_gameActions) {
		action->setDisabled(true);
	}
	setWindowFilePath(QString());
	updateTitle();
	detachWidget(m_display);
	m_screenWidget->setDimensions(m_logo.width(), m_logo.height());
	m_screenWidget->setLockIntegerScaling(false);
	m_screenWidget->setLockAspectRatio(true);
	m_screenWidget->setPixmap(m_logo);
	m_screenWidget->unsetCursor();
#ifdef M_CORE_GB
	m_display->setMinimumSize(GB_VIDEO_HORIZONTAL_PIXELS, GB_VIDEO_VERTICAL_PIXELS);
#elif defined(M_CORE_GBA)
	m_display->setMinimumSize(VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
#endif
	m_screenWidget->setMinimumSize(m_display->minimumSize());

	m_videoLayers->clear();
	m_audioChannels->clear();

	m_fpsTimer.stop();
	m_focusCheck.stop();
}

void Window::gameCrashed(const QString& errorMessage) {
	QMessageBox* crash = new QMessageBox(QMessageBox::Critical, tr("Crash"),
	                                     tr("The game has crashed with the following error:\n\n%1").arg(errorMessage),
	                                     QMessageBox::Ok, this, Qt::Sheet);
	crash->setAttribute(Qt::WA_DeleteOnClose);
	crash->show();
	connect(m_controller, &GameController::gameStarted, crash, &QWidget::close);
}

void Window::gameFailed() {
	QMessageBox* fail = new QMessageBox(QMessageBox::Warning, tr("Couldn't Load"),
	                                    tr("Could not load game. Are you sure it's in the correct format?"),
	                                    QMessageBox::Ok, this, Qt::Sheet);
	fail->setAttribute(Qt::WA_DeleteOnClose);
	fail->show();
	connect(m_controller, &GameController::gameStarted, fail, &QWidget::close);
}

void Window::unimplementedBiosCall(int call) {
	if (m_hitUnimplementedBiosCall) {
		return;
	}
	m_hitUnimplementedBiosCall = true;

	QMessageBox* fail = new QMessageBox(
	    QMessageBox::Warning, tr("Unimplemented BIOS call"),
	    tr("This game uses a BIOS call that is not implemented. Please use the official BIOS for best experience."),
	    QMessageBox::Ok, this, Qt::Sheet);
	fail->setAttribute(Qt::WA_DeleteOnClose);
	fail->show();
}

void Window::tryMakePortable() {
	QMessageBox* confirm = new QMessageBox(QMessageBox::Question, tr("Really make portable?"),
	                                       tr("This will make the emulator load its configuration from the same directory as the executable. Do you want to continue?"),
	                                       QMessageBox::Yes | QMessageBox::Cancel, this, Qt::Sheet);
	confirm->setAttribute(Qt::WA_DeleteOnClose);
	connect(confirm->button(QMessageBox::Yes), &QAbstractButton::clicked, m_config, &ConfigController::makePortable);
	confirm->show();
}

void Window::mustRestart() {
	QMessageBox* dialog = new QMessageBox(QMessageBox::Warning, tr("Restart needed"),
	                                      tr("Some changes will not take effect until the emulator is restarted."),
	                                      QMessageBox::Ok, this, Qt::Sheet);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->show();
}

void Window::recordFrame() {
	m_frameList.append(QDateTime::currentDateTime());
	while (m_frameList.count() > FRAME_LIST_SIZE) {
		m_frameList.removeFirst();
	}
}

void Window::showFPS() {
	if (m_frameList.isEmpty()) {
		updateTitle();
		return;
	}
	qint64 interval = m_frameList.first().msecsTo(m_frameList.last());
	float fps = (m_frameList.count() - 1) * 10000.f / interval;
	fps = round(fps) / 10.f;
	updateTitle(fps);
}

void Window::updateTitle(float fps) {
	QString title;

	m_controller->threadInterrupt();
	if (m_controller->isLoaded()) {
		const NoIntroDB* db = GBAApp::app()->gameDB();
		NoIntroGame game{};
		uint32_t crc32 = 0;
		m_controller->thread()->core->checksum(m_controller->thread()->core, &crc32, CHECKSUM_CRC32);

		char gameTitle[17] = { '\0' };
		mCore* core = m_controller->thread()->core;
		core->getGameTitle(core, gameTitle);
		title = gameTitle;

#ifdef USE_SQLITE3
		if (db && crc32 && NoIntroDBLookupGameByCRC(db, crc32, &game)) {
			title = QLatin1String(game.name);
		}
#endif
	}
	MultiplayerController* multiplayer = m_controller->multiplayerController();
	if (multiplayer && multiplayer->attached() > 1) {
		title += tr(" -  Player %1 of %2").arg(multiplayer->playerId(m_controller) + 1).arg(multiplayer->attached());
		for (QAction* action : m_nonMpActions) {
			action->setDisabled(true);
		}
	} else if (m_controller->isLoaded()) {
		for (QAction* action : m_nonMpActions) {
			action->setDisabled(false);
		}
	}
	m_controller->threadContinue();
	if (title.isNull()) {
		setWindowTitle(tr("%1 - %2").arg(projectName).arg(projectVersion));
	} else if (fps < 0) {
		setWindowTitle(tr("%1 - %2 - %3").arg(projectName).arg(title).arg(projectVersion));
	} else {
		setWindowTitle(tr("%1 - %2 (%3 fps) - %4").arg(projectName).arg(title).arg(fps).arg(projectVersion));
	}
}

void Window::openStateWindow(LoadSave ls) {
	if (m_stateWindow) {
		return;
	}
	MultiplayerController* multiplayer = m_controller->multiplayerController();
	if (multiplayer && multiplayer->attached() > 1) {
		return;
	}
	bool wasPaused = m_controller->isPaused();
	m_stateWindow = new LoadSaveState(m_controller);
	connect(this, &Window::shutdown, m_stateWindow, &QWidget::close);
	connect(m_controller, &GameController::gameStopped, m_stateWindow, &QWidget::close);
	connect(m_stateWindow, &LoadSaveState::closed, [this]() {
		detachWidget(m_stateWindow);
		m_stateWindow = nullptr;
		QMetaObject::invokeMethod(this, "setFocus", Qt::QueuedConnection);
	});
	if (!wasPaused) {
		m_controller->setPaused(true);
		connect(m_stateWindow, &LoadSaveState::closed, [this]() { m_controller->setPaused(false); });
	}
	m_stateWindow->setAttribute(Qt::WA_DeleteOnClose);
	m_stateWindow->setMode(ls);
	attachWidget(m_stateWindow);
}

void Window::setupMenu(QMenuBar* menubar) {
	menubar->clear();
	QMenu* fileMenu = menubar->addMenu(tr("&File"));
	m_shortcutController->addMenu(fileMenu);
	installEventFilter(m_shortcutController);
	addControlledAction(fileMenu, fileMenu->addAction(tr("Load &ROM..."), this, SLOT(selectROM()), QKeySequence::Open),
	                    "loadROM");
#ifdef USE_SQLITE3
	addControlledAction(fileMenu, fileMenu->addAction(tr("Load ROM in archive..."), this, SLOT(selectROMInArchive())),
	                    "loadROMInArchive");
	addControlledAction(fileMenu, fileMenu->addAction(tr("Add folder to library..."), this, SLOT(addDirToLibrary())),
	                    "addDirToLibrary");
#endif

	QAction* loadTemporarySave = new QAction(tr("Load temporary save..."), fileMenu);
	connect(loadTemporarySave, &QAction::triggered, [this]() { this->selectSave(true); });
	m_gameActions.append(loadTemporarySave);
	addControlledAction(fileMenu, loadTemporarySave, "loadTemporarySave");

	addControlledAction(fileMenu, fileMenu->addAction(tr("Load &patch..."), this, SLOT(selectPatch())), "loadPatch");

	QAction* bootBIOS = new QAction(tr("Boot BIOS"), fileMenu);
	connect(bootBIOS, &QAction::triggered, [this]() {
		m_controller->loadBIOS(PLATFORM_GBA, m_config->getOption("gba.bios"));
		m_controller->bootBIOS();
	});
	addControlledAction(fileMenu, bootBIOS, "bootBIOS");

	addControlledAction(fileMenu, fileMenu->addAction(tr("Replace ROM..."), this, SLOT(replaceROM())), "replaceROM");

	QAction* romInfo = new QAction(tr("ROM &info..."), fileMenu);
	connect(romInfo, &QAction::triggered, openTView<ROMInfo>());
	m_gameActions.append(romInfo);
	addControlledAction(fileMenu, romInfo, "romInfo");

	m_mruMenu = fileMenu->addMenu(tr("Recent"));

	fileMenu->addSeparator();

	addControlledAction(fileMenu, fileMenu->addAction(tr("Make portable"), this, SLOT(tryMakePortable())), "makePortable");

	fileMenu->addSeparator();

	QAction* loadState = new QAction(tr("&Load state"), fileMenu);
	loadState->setShortcut(tr("F10"));
	connect(loadState, &QAction::triggered, [this]() { this->openStateWindow(LoadSave::LOAD); });
	m_gameActions.append(loadState);
	m_nonMpActions.append(loadState);
	addControlledAction(fileMenu, loadState, "loadState");

	QAction* saveState = new QAction(tr("&Save state"), fileMenu);
	saveState->setShortcut(tr("Shift+F10"));
	connect(saveState, &QAction::triggered, [this]() { this->openStateWindow(LoadSave::SAVE); });
	m_gameActions.append(saveState);
	m_nonMpActions.append(saveState);
	addControlledAction(fileMenu, saveState, "saveState");

	QMenu* quickLoadMenu = fileMenu->addMenu(tr("Quick load"));
	QMenu* quickSaveMenu = fileMenu->addMenu(tr("Quick save"));
	m_shortcutController->addMenu(quickLoadMenu);
	m_shortcutController->addMenu(quickSaveMenu);

	QAction* quickLoad = new QAction(tr("Load recent"), quickLoadMenu);
	connect(quickLoad, &QAction::triggered, m_controller, &GameController::loadState);
	m_gameActions.append(quickLoad);
	m_nonMpActions.append(quickLoad);
	addControlledAction(quickLoadMenu, quickLoad, "quickLoad");

	QAction* quickSave = new QAction(tr("Save recent"), quickSaveMenu);
	connect(quickSave, &QAction::triggered, m_controller, &GameController::saveState);
	m_gameActions.append(quickSave);
	m_nonMpActions.append(quickSave);
	addControlledAction(quickSaveMenu, quickSave, "quickSave");

	quickLoadMenu->addSeparator();
	quickSaveMenu->addSeparator();

	QAction* undoLoadState = new QAction(tr("Undo load state"), quickLoadMenu);
	undoLoadState->setShortcut(tr("F11"));
	connect(undoLoadState, &QAction::triggered, m_controller, &GameController::loadBackupState);
	m_gameActions.append(undoLoadState);
	m_nonMpActions.append(undoLoadState);
	addControlledAction(quickLoadMenu, undoLoadState, "undoLoadState");

	QAction* undoSaveState = new QAction(tr("Undo save state"), quickSaveMenu);
	undoSaveState->setShortcut(tr("Shift+F11"));
	connect(undoSaveState, &QAction::triggered, m_controller, &GameController::saveBackupState);
	m_gameActions.append(undoSaveState);
	m_nonMpActions.append(undoSaveState);
	addControlledAction(quickSaveMenu, undoSaveState, "undoSaveState");

	quickLoadMenu->addSeparator();
	quickSaveMenu->addSeparator();

	int i;
	for (i = 1; i < 10; ++i) {
		quickLoad = new QAction(tr("State &%1").arg(i), quickLoadMenu);
		quickLoad->setShortcut(tr("F%1").arg(i));
		connect(quickLoad, &QAction::triggered, [this, i]() { m_controller->loadState(i); });
		m_gameActions.append(quickLoad);
		m_nonMpActions.append(quickLoad);
		addControlledAction(quickLoadMenu, quickLoad, QString("quickLoad.%1").arg(i));

		quickSave = new QAction(tr("State &%1").arg(i), quickSaveMenu);
		quickSave->setShortcut(tr("Shift+F%1").arg(i));
		connect(quickSave, &QAction::triggered, [this, i]() { m_controller->saveState(i); });
		m_gameActions.append(quickSave);
		m_nonMpActions.append(quickSave);
		addControlledAction(quickSaveMenu, quickSave, QString("quickSave.%1").arg(i));
	}

#ifdef M_CORE_GBA
	fileMenu->addSeparator();
	QAction* importShark = new QAction(tr("Import GameShark Save"), fileMenu);
	connect(importShark, &QAction::triggered, this, &Window::importSharkport);
	m_gameActions.append(importShark);
	m_gbaActions.append(importShark);
	addControlledAction(fileMenu, importShark, "importShark");

	QAction* exportShark = new QAction(tr("Export GameShark Save"), fileMenu);
	connect(exportShark, &QAction::triggered, this, &Window::exportSharkport);
	m_gameActions.append(exportShark);
	m_gbaActions.append(exportShark);
	addControlledAction(fileMenu, exportShark, "exportShark");
#endif

	fileMenu->addSeparator();
	m_multiWindow = new QAction(tr("New multiplayer window"), fileMenu);
	connect(m_multiWindow, &QAction::triggered, [this]() {
		GBAApp::app()->newWindow();
	});
	addControlledAction(fileMenu, m_multiWindow, "multiWindow");

#ifndef Q_OS_MAC
	fileMenu->addSeparator();
#endif

	QAction* about = new QAction(tr("About"), fileMenu);
	connect(about, &QAction::triggered, this, &Window::openAboutScreen);
	fileMenu->addAction(about);

#ifndef Q_OS_MAC
	addControlledAction(fileMenu, fileMenu->addAction(tr("E&xit"), this, SLOT(close()), QKeySequence::Quit), "quit");
#endif

	QMenu* emulationMenu = menubar->addMenu(tr("&Emulation"));
	m_shortcutController->addMenu(emulationMenu);
	QAction* reset = new QAction(tr("&Reset"), emulationMenu);
	reset->setShortcut(tr("Ctrl+R"));
	connect(reset, &QAction::triggered, m_controller, &GameController::reset);
	m_gameActions.append(reset);
	addControlledAction(emulationMenu, reset, "reset");

	QAction* shutdown = new QAction(tr("Sh&utdown"), emulationMenu);
	connect(shutdown, &QAction::triggered, m_controller, &GameController::closeGame);
	m_gameActions.append(shutdown);
	addControlledAction(emulationMenu, shutdown, "shutdown");

#ifdef M_CORE_GBA
	QAction* yank = new QAction(tr("Yank game pak"), emulationMenu);
	connect(yank, &QAction::triggered, m_controller, &GameController::yankPak);
	m_gameActions.append(yank);
	m_gbaActions.append(yank);
	addControlledAction(emulationMenu, yank, "yank");
#endif
	emulationMenu->addSeparator();

	QAction* pause = new QAction(tr("&Pause"), emulationMenu);
	pause->setChecked(false);
	pause->setCheckable(true);
	pause->setShortcut(tr("Ctrl+P"));
	connect(pause, &QAction::triggered, m_controller, &GameController::setPaused);
	connect(m_controller, &GameController::gamePaused, [this, pause]() {
		pause->setChecked(true);
	});
	connect(m_controller, &GameController::gameUnpaused, [pause]() { pause->setChecked(false); });
	m_gameActions.append(pause);
	addControlledAction(emulationMenu, pause, "pause");

	QAction* frameAdvance = new QAction(tr("&Next frame"), emulationMenu);
	frameAdvance->setShortcut(tr("Ctrl+N"));
	connect(frameAdvance, &QAction::triggered, m_controller, &GameController::frameAdvance);
	m_gameActions.append(frameAdvance);
	addControlledAction(emulationMenu, frameAdvance, "frameAdvance");

	emulationMenu->addSeparator();

	m_shortcutController->addFunctions(emulationMenu, [this]() {
		m_controller->setTurbo(true, false);
	}, [this]() {
		m_controller->setTurbo(false, false);
	}, QKeySequence(Qt::Key_Tab), tr("Fast forward (held)"), "holdFastForward");

	QAction* turbo = new QAction(tr("&Fast forward"), emulationMenu);
	turbo->setCheckable(true);
	turbo->setChecked(false);
	turbo->setShortcut(tr("Shift+Tab"));
	connect(turbo, SIGNAL(triggered(bool)), m_controller, SLOT(setTurbo(bool)));
	addControlledAction(emulationMenu, turbo, "fastForward");

	QMenu* ffspeedMenu = emulationMenu->addMenu(tr("Fast forward speed"));
	ConfigOption* ffspeed = m_config->addOption("fastForwardRatio");
	ffspeed->connect([this](const QVariant& value) {
		m_controller->setTurboSpeed(value.toFloat());
	}, this);
	ffspeed->addValue(tr("Unbounded"), -1.0f, ffspeedMenu);
	ffspeed->setValue(QVariant(-1.0f));
	ffspeedMenu->addSeparator();
	for (i = 2; i < 11; ++i) {
		ffspeed->addValue(tr("%0x").arg(i), i, ffspeedMenu);
	}
	m_config->updateOption("fastForwardRatio");

	m_shortcutController->addFunctions(emulationMenu, [this]() {
		m_controller->startRewinding();
	}, [this]() {
		m_controller->stopRewinding();
	}, QKeySequence("`"), tr("Rewind (held)"), "holdRewind");

	QAction* rewind = new QAction(tr("Re&wind"), emulationMenu);
	rewind->setShortcut(tr("~"));
	connect(rewind, &QAction::triggered, m_controller, &GameController::rewind);
	m_gameActions.append(rewind);
	m_nonMpActions.append(rewind);
	addControlledAction(emulationMenu, rewind, "rewind");

	QAction* frameRewind = new QAction(tr("Step backwards"), emulationMenu);
	frameRewind->setShortcut(tr("Ctrl+B"));
	connect(frameRewind, &QAction::triggered, [this] () {
		m_controller->rewind(1);
	});
	m_gameActions.append(frameRewind);
	m_nonMpActions.append(frameRewind);
	addControlledAction(emulationMenu, frameRewind, "frameRewind");

	ConfigOption* videoSync = m_config->addOption("videoSync");
	videoSync->addBoolean(tr("Sync to &video"), emulationMenu);
	videoSync->connect([this](const QVariant& value) {
		m_controller->setVideoSync(value.toBool());
	}, this);
	m_config->updateOption("videoSync");

	ConfigOption* audioSync = m_config->addOption("audioSync");
	audioSync->addBoolean(tr("Sync to &audio"), emulationMenu);
	audioSync->connect([this](const QVariant& value) {
		m_controller->setAudioSync(value.toBool());
	}, this);
	m_config->updateOption("audioSync");

	emulationMenu->addSeparator();

	QMenu* solarMenu = emulationMenu->addMenu(tr("Solar sensor"));
	m_shortcutController->addMenu(solarMenu);
	QAction* solarIncrease = new QAction(tr("Increase solar level"), solarMenu);
	connect(solarIncrease, &QAction::triggered, m_controller, &GameController::increaseLuminanceLevel);
	addControlledAction(solarMenu, solarIncrease, "increaseLuminanceLevel");

	QAction* solarDecrease = new QAction(tr("Decrease solar level"), solarMenu);
	connect(solarDecrease, &QAction::triggered, m_controller, &GameController::decreaseLuminanceLevel);
	addControlledAction(solarMenu, solarDecrease, "decreaseLuminanceLevel");

	QAction* maxSolar = new QAction(tr("Brightest solar level"), solarMenu);
	connect(maxSolar, &QAction::triggered, [this]() { m_controller->setLuminanceLevel(10); });
	addControlledAction(solarMenu, maxSolar, "maxLuminanceLevel");

	QAction* minSolar = new QAction(tr("Darkest solar level"), solarMenu);
	connect(minSolar, &QAction::triggered, [this]() { m_controller->setLuminanceLevel(0); });
	addControlledAction(solarMenu, minSolar, "minLuminanceLevel");

	solarMenu->addSeparator();
	for (int i = 0; i <= 10; ++i) {
		QAction* setSolar = new QAction(tr("Brightness %1").arg(QString::number(i)), solarMenu);
		connect(setSolar, &QAction::triggered, [this, i]() {
			m_controller->setLuminanceLevel(i);
		});
		addControlledAction(solarMenu, setSolar, QString("luminanceLevel.%1").arg(QString::number(i)));
	}

	QMenu* avMenu = menubar->addMenu(tr("Audio/&Video"));
	m_shortcutController->addMenu(avMenu);
	QMenu* frameMenu = avMenu->addMenu(tr("Frame size"));
	m_shortcutController->addMenu(frameMenu, avMenu);
	for (int i = 1; i <= 6; ++i) {
		QAction* setSize = new QAction(tr("%1x").arg(QString::number(i)), avMenu);
		setSize->setCheckable(true);
		if (m_savedScale == i) {
			setSize->setChecked(true);
		}
		connect(setSize, &QAction::triggered, [this, i, setSize]() {
			showNormal();
			QSize size(VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
			if (m_controller->isLoaded()) {
				size = m_controller->screenDimensions();
			}
			size *= i;
			m_savedScale = i;
			m_config->setOption("scaleMultiplier", i); // TODO: Port to other
			resizeFrame(size);
			bool enableSignals = setSize->blockSignals(true);
			setSize->setChecked(true);
			setSize->blockSignals(enableSignals);
		});
		m_frameSizes[i] = setSize;
		addControlledAction(frameMenu, setSize, QString("frame%1x").arg(QString::number(i)));
	}
	QKeySequence fullscreenKeys;
#ifdef Q_OS_WIN
	fullscreenKeys = QKeySequence("Alt+Return");
#else
	fullscreenKeys = QKeySequence("Ctrl+F");
#endif
	addControlledAction(frameMenu, frameMenu->addAction(tr("Toggle fullscreen"), this, SLOT(toggleFullScreen()), fullscreenKeys), "fullscreen");

	ConfigOption* lockAspectRatio = m_config->addOption("lockAspectRatio");
	lockAspectRatio->addBoolean(tr("Lock aspect ratio"), avMenu);
	lockAspectRatio->connect([this](const QVariant& value) {
		m_display->lockAspectRatio(value.toBool());
		if (m_controller->isLoaded()) {
			m_screenWidget->setLockAspectRatio(value.toBool());
		}
	}, this);
	m_config->updateOption("lockAspectRatio");

	ConfigOption* lockIntegerScaling = m_config->addOption("lockIntegerScaling");
	lockIntegerScaling->addBoolean(tr("Force integer scaling"), avMenu);
	lockIntegerScaling->connect([this](const QVariant& value) {
		m_display->lockIntegerScaling(value.toBool());
		if (m_controller->isLoaded()) {
			m_screenWidget->setLockIntegerScaling(value.toBool());
		}
	}, this);
	m_config->updateOption("lockIntegerScaling");

	ConfigOption* resampleVideo = m_config->addOption("resampleVideo");
	resampleVideo->addBoolean(tr("Bilinear filtering"), avMenu);
	resampleVideo->connect([this](const QVariant& value) {
		m_display->filter(value.toBool());
	}, this);
	m_config->updateOption("resampleVideo");

	QMenu* skipMenu = avMenu->addMenu(tr("Frame&skip"));
	ConfigOption* skip = m_config->addOption("frameskip");
	skip->connect([this](const QVariant& value) {
		reloadConfig();
	}, this);
	for (int i = 0; i <= 10; ++i) {
		skip->addValue(QString::number(i), i, skipMenu);
	}
	m_config->updateOption("frameskip");

	avMenu->addSeparator();

	ConfigOption* mute = m_config->addOption("mute");
	QAction* muteAction = mute->addBoolean(tr("Mute"), avMenu);
	mute->connect([this](const QVariant& value) {
		reloadConfig();
	}, this);
	m_config->updateOption("mute");
	addControlledAction(avMenu, muteAction, "mute");

	QMenu* target = avMenu->addMenu(tr("FPS target"));
	ConfigOption* fpsTargetOption = m_config->addOption("fpsTarget");
	fpsTargetOption->connect([this](const QVariant& value) {
		emit fpsTargetChanged(value.toFloat());
	}, this);
	fpsTargetOption->addValue(tr("15"), 15, target);
	fpsTargetOption->addValue(tr("30"), 30, target);
	fpsTargetOption->addValue(tr("45"), 45, target);
	fpsTargetOption->addValue(tr("Native (59.7)"), float(GBA_ARM7TDMI_FREQUENCY) / float(VIDEO_TOTAL_LENGTH), target);
	fpsTargetOption->addValue(tr("60"), 60, target);
	fpsTargetOption->addValue(tr("90"), 90, target);
	fpsTargetOption->addValue(tr("120"), 120, target);
	fpsTargetOption->addValue(tr("240"), 240, target);
	m_config->updateOption("fpsTarget");

#if defined(USE_PNG) || defined(USE_FFMPEG) || defined(USE_MAGICK)
	avMenu->addSeparator();
#endif

#ifdef USE_PNG
	QAction* screenshot = new QAction(tr("Take &screenshot"), avMenu);
	screenshot->setShortcut(tr("F12"));
	connect(screenshot, &QAction::triggered, m_controller, &GameController::screenshot);
	m_gameActions.append(screenshot);
	addControlledAction(avMenu, screenshot, "screenshot");
#endif

#ifdef USE_FFMPEG
	QAction* recordOutput = new QAction(tr("Record output..."), avMenu);
	connect(recordOutput, &QAction::triggered, this, &Window::openVideoWindow);
	addControlledAction(avMenu, recordOutput, "recordOutput");
	m_gameActions.append(recordOutput);
#endif

#ifdef USE_MAGICK
	QAction* recordGIF = new QAction(tr("Record GIF..."), avMenu);
	connect(recordGIF, &QAction::triggered, this, &Window::openGIFWindow);
	addControlledAction(avMenu, recordGIF, "recordGIF");
#endif

	QAction* recordVL = new QAction(tr("Record video log..."), avMenu);
	connect(recordVL, &QAction::triggered, this, &Window::startVideoLog);
	addControlledAction(avMenu, recordVL, "recordVL");
	m_gameActions.append(recordVL);

	QAction* stopVL = new QAction(tr("Stop video log"), avMenu);
	connect(stopVL, &QAction::triggered, m_controller, &GameController::endVideoLog);
	addControlledAction(avMenu, stopVL, "stopVL");
	m_gameActions.append(stopVL);

	avMenu->addSeparator();
	m_videoLayers = avMenu->addMenu(tr("Video layers"));
	m_shortcutController->addMenu(m_videoLayers, avMenu);

	m_audioChannels = avMenu->addMenu(tr("Audio channels"));
	m_shortcutController->addMenu(m_audioChannels, avMenu);

	QMenu* toolsMenu = menubar->addMenu(tr("&Tools"));
	m_shortcutController->addMenu(toolsMenu);
	QAction* viewLogs = new QAction(tr("View &logs..."), toolsMenu);
	connect(viewLogs, &QAction::triggered, m_logView, &QWidget::show);
	addControlledAction(toolsMenu, viewLogs, "viewLogs");

	QAction* overrides = new QAction(tr("Game &overrides..."), toolsMenu);
	connect(overrides, &QAction::triggered, openTView<OverrideView, ConfigController*>(m_config));
	addControlledAction(toolsMenu, overrides, "overrideWindow");

	QAction* sensors = new QAction(tr("Game &Pak sensors..."), toolsMenu);
	connect(sensors, &QAction::triggered, openTView<SensorView, InputController*>(&m_inputController));
	addControlledAction(toolsMenu, sensors, "sensorWindow");

	QAction* cheats = new QAction(tr("&Cheats..."), toolsMenu);
	connect(cheats, &QAction::triggered, openTView<CheatsView>());
	m_gameActions.append(cheats);
	addControlledAction(toolsMenu, cheats, "cheatsWindow");

	toolsMenu->addSeparator();
	addControlledAction(toolsMenu, toolsMenu->addAction(tr("Settings..."), this, SLOT(openSettingsWindow())),
	                    "settings");

	toolsMenu->addSeparator();

#ifdef USE_DEBUGGERS
	QAction* consoleWindow = new QAction(tr("Open debugger console..."), toolsMenu);
	connect(consoleWindow, &QAction::triggered, this, &Window::consoleOpen);
	addControlledAction(toolsMenu, consoleWindow, "debuggerWindow");
#endif

#ifdef USE_GDB_STUB
	QAction* gdbWindow = new QAction(tr("Start &GDB server..."), toolsMenu);
	connect(gdbWindow, &QAction::triggered, this, &Window::gdbOpen);
	m_gbaActions.append(gdbWindow);
	addControlledAction(toolsMenu, gdbWindow, "gdbWindow");
#endif
	toolsMenu->addSeparator();

	QAction* paletteView = new QAction(tr("View &palette..."), toolsMenu);
	connect(paletteView, &QAction::triggered, openTView<PaletteView>());
	m_gameActions.append(paletteView);
	addControlledAction(toolsMenu, paletteView, "paletteWindow");

	QAction* objView = new QAction(tr("View &sprites..."), toolsMenu);
	connect(objView, &QAction::triggered, openTView<ObjView>());
	m_gameActions.append(objView);
	addControlledAction(toolsMenu, objView, "spriteWindow");

	QAction* tileView = new QAction(tr("View &tiles..."), toolsMenu);
	connect(tileView, &QAction::triggered, openTView<TileView>());
	m_gameActions.append(tileView);
	addControlledAction(toolsMenu, tileView, "tileWindow");

	QAction* memoryView = new QAction(tr("View memory..."), toolsMenu);
	connect(memoryView, &QAction::triggered, openTView<MemoryView>());
	m_gameActions.append(memoryView);
	addControlledAction(toolsMenu, memoryView, "memoryView");

	QAction* memorySearch = new QAction(tr("Search memory..."), toolsMenu);
	connect(memorySearch, &QAction::triggered, openTView<MemorySearch>());
	m_gameActions.append(memorySearch);
	addControlledAction(toolsMenu, memorySearch, "memorySearch");

#ifdef M_CORE_GBA
	QAction* ioViewer = new QAction(tr("View &I/O registers..."), toolsMenu);
	connect(ioViewer, &QAction::triggered, openTView<IOViewer>());
	m_gameActions.append(ioViewer);
	m_gbaActions.append(ioViewer);
	addControlledAction(toolsMenu, ioViewer, "ioViewer");
#endif

	ConfigOption* skipBios = m_config->addOption("skipBios");
	skipBios->connect([this](const QVariant& value) {
		reloadConfig();
	}, this);

	ConfigOption* useBios = m_config->addOption("useBios");
	useBios->connect([this](const QVariant& value) {
		m_controller->setUseBIOS(value.toBool());
	}, this);

	ConfigOption* buffers = m_config->addOption("audioBuffers");
	buffers->connect([this](const QVariant& value) {
		emit audioBufferSamplesChanged(value.toInt());
	}, this);

	ConfigOption* sampleRate = m_config->addOption("sampleRate");
	sampleRate->connect([this](const QVariant& value) {
		emit sampleRateChanged(value.toUInt());
	}, this);

	ConfigOption* volume = m_config->addOption("volume");
	volume->connect([this](const QVariant& value) {
		reloadConfig();
	}, this);

	ConfigOption* rewindEnable = m_config->addOption("rewindEnable");
	rewindEnable->connect([this](const QVariant& value) {
		m_controller->setRewind(value.toBool(), m_config->getOption("rewindBufferCapacity").toInt(), m_config->getOption("rewindSave").toInt());
	}, this);

	ConfigOption* rewindBufferCapacity = m_config->addOption("rewindBufferCapacity");
	rewindBufferCapacity->connect([this](const QVariant& value) {
		m_controller->setRewind(m_config->getOption("rewindEnable").toInt(), value.toInt(), m_config->getOption("rewindSave").toInt());
	}, this);

	ConfigOption* rewindSave = m_config->addOption("rewindSave");
	rewindBufferCapacity->connect([this](const QVariant& value) {
		m_controller->setRewind(m_config->getOption("rewindEnable").toInt(), m_config->getOption("rewindBufferCapacity").toInt(), value.toBool());
	}, this);

	ConfigOption* allowOpposingDirections = m_config->addOption("allowOpposingDirections");
	allowOpposingDirections->connect([this](const QVariant& value) {
		m_inputController.setAllowOpposing(value.toBool());
	}, this);

	ConfigOption* saveStateExtdata = m_config->addOption("saveStateExtdata");
	saveStateExtdata->connect([this](const QVariant& value) {
		m_controller->setSaveStateExtdata(value.toInt());
	}, this);
	m_config->updateOption("saveStateExtdata");

	ConfigOption* loadStateExtdata = m_config->addOption("loadStateExtdata");
	loadStateExtdata->connect([this](const QVariant& value) {
		m_controller->setLoadStateExtdata(value.toInt());
	}, this);
	m_config->updateOption("loadStateExtdata");

	ConfigOption* preload = m_config->addOption("preload");
	preload->connect([this](const QVariant& value) {
		m_controller->setPreload(value.toBool());
	}, this);
	m_config->updateOption("preload");

	QAction* exitFullScreen = new QAction(tr("Exit fullscreen"), frameMenu);
	connect(exitFullScreen, &QAction::triggered, this, &Window::exitFullScreen);
	exitFullScreen->setShortcut(QKeySequence("Esc"));
	addHiddenAction(frameMenu, exitFullScreen, "exitFullScreen");

	QMenu* autofireMenu = new QMenu(tr("Autofire"), this);
	m_shortcutController->addMenu(autofireMenu);

	m_shortcutController->addFunctions(autofireMenu, [this]() {
		m_controller->setAutofire(GBA_KEY_A, true);
	}, [this]() {
		m_controller->setAutofire(GBA_KEY_A, false);
	}, QKeySequence(), tr("Autofire A"), "autofireA");

	m_shortcutController->addFunctions(autofireMenu, [this]() {
		m_controller->setAutofire(GBA_KEY_B, true);
	}, [this]() {
		m_controller->setAutofire(GBA_KEY_B, false);
	}, QKeySequence(), tr("Autofire B"), "autofireB");

	m_shortcutController->addFunctions(autofireMenu, [this]() {
		m_controller->setAutofire(GBA_KEY_L, true);
	}, [this]() {
		m_controller->setAutofire(GBA_KEY_L, false);
	}, QKeySequence(), tr("Autofire L"), "autofireL");

	m_shortcutController->addFunctions(autofireMenu, [this]() {
		m_controller->setAutofire(GBA_KEY_R, true);
	}, [this]() {
		m_controller->setAutofire(GBA_KEY_R, false);
	}, QKeySequence(), tr("Autofire R"), "autofireR");

	m_shortcutController->addFunctions(autofireMenu, [this]() {
		m_controller->setAutofire(GBA_KEY_START, true);
	}, [this]() {
		m_controller->setAutofire(GBA_KEY_START, false);
	}, QKeySequence(), tr("Autofire Start"), "autofireStart");

	m_shortcutController->addFunctions(autofireMenu, [this]() {
		m_controller->setAutofire(GBA_KEY_SELECT, true);
	}, [this]() {
		m_controller->setAutofire(GBA_KEY_SELECT, false);
	}, QKeySequence(), tr("Autofire Select"), "autofireSelect");

	m_shortcutController->addFunctions(autofireMenu, [this]() {
		m_controller->setAutofire(GBA_KEY_UP, true);
	}, [this]() {
		m_controller->setAutofire(GBA_KEY_UP, false);
	}, QKeySequence(), tr("Autofire Up"), "autofireUp");

	m_shortcutController->addFunctions(autofireMenu, [this]() {
		m_controller->setAutofire(GBA_KEY_RIGHT, true);
	}, [this]() {
		m_controller->setAutofire(GBA_KEY_RIGHT, false);
	}, QKeySequence(), tr("Autofire Right"), "autofireRight");

	m_shortcutController->addFunctions(autofireMenu, [this]() {
		m_controller->setAutofire(GBA_KEY_DOWN, true);
	}, [this]() {
		m_controller->setAutofire(GBA_KEY_DOWN, false);
	}, QKeySequence(), tr("Autofire Down"), "autofireDown");

	m_shortcutController->addFunctions(autofireMenu, [this]() {
		m_controller->setAutofire(GBA_KEY_LEFT, true);
	}, [this]() {
		m_controller->setAutofire(GBA_KEY_LEFT, false);
	}, QKeySequence(), tr("Autofire Left"), "autofireLeft");

	for (QAction* action : m_gameActions) {
		action->setDisabled(true);
	}
}

void Window::attachWidget(QWidget* widget) {
	m_screenWidget->layout()->addWidget(widget);
	m_screenWidget->unsetCursor();
	static_cast<QStackedLayout*>(m_screenWidget->layout())->setCurrentWidget(widget);
}

void Window::detachWidget(QWidget* widget) {
	m_screenWidget->layout()->removeWidget(widget);
}

void Window::appendMRU(const QString& fname) {
	int index = m_mruFiles.indexOf(fname);
	if (index >= 0) {
		m_mruFiles.removeAt(index);
	}
	m_mruFiles.prepend(fname);
	while (m_mruFiles.size() > ConfigController::MRU_LIST_SIZE) {
		m_mruFiles.removeLast();
	}
	updateMRU();
}

void Window::updateMRU() {
	if (!m_mruMenu) {
		return;
	}
	for (QAction* action : m_mruMenu->actions()) {
		delete action;
	}
	m_mruMenu->clear();
	int i = 0;
	for (const QString& file : m_mruFiles) {
		QAction* item = new QAction(QDir::toNativeSeparators(file).replace("&", "&&"), m_mruMenu);
		item->setShortcut(QString("Ctrl+%1").arg(i));
		connect(item, &QAction::triggered, [this, file]() { m_controller->loadGame(file); });
		m_mruMenu->addAction(item);
		++i;
	}
	m_config->setMRU(m_mruFiles);
	m_config->write();
	m_mruMenu->setEnabled(i > 0);
}

QAction* Window::addControlledAction(QMenu* menu, QAction* action, const QString& name) {
	addHiddenAction(menu, action, name);
	menu->addAction(action);
	return action;
}

QAction* Window::addHiddenAction(QMenu* menu, QAction* action, const QString& name) {
	m_shortcutController->addAction(menu, action, name);
	action->setShortcutContext(Qt::WidgetShortcut);
	addAction(action);
	return action;
}

void Window::focusCheck() {
	if (!m_config->getOption("pauseOnFocusLost").toInt()) {
		return;
	}
	if (QGuiApplication::focusWindow() && m_autoresume) {
		m_controller->setPaused(false);
		m_autoresume = false;
	} else if (!QGuiApplication::focusWindow() && !m_controller->isPaused()) {
		m_autoresume = true;
		m_controller->setPaused(true);
	}
}

WindowBackground::WindowBackground(QWidget* parent)
	: QLabel(parent)
{
	setLayout(new QStackedLayout());
	layout()->setContentsMargins(0, 0, 0, 0);
	setAlignment(Qt::AlignCenter);
}

void WindowBackground::setSizeHint(const QSize& hint) {
	m_sizeHint = hint;
}

QSize WindowBackground::sizeHint() const {
	return m_sizeHint;
}

void WindowBackground::setDimensions(int width, int height) {
	m_aspectWidth = width;
	m_aspectHeight = height;
}

void WindowBackground::setLockIntegerScaling(bool lock) {
	m_lockIntegerScaling = lock;
}

void WindowBackground::setLockAspectRatio(bool lock) {
	m_lockAspectRatio = lock;
}

void WindowBackground::paintEvent(QPaintEvent*) {
	const QPixmap* logo = pixmap();
	if (!logo) {
		return;
	}
	QPainter painter(this);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);
	painter.fillRect(QRect(QPoint(), size()), Qt::black);
	QSize s = size();
	QSize ds = s;
	if (m_lockAspectRatio) {
		if (ds.width() * m_aspectHeight > ds.height() * m_aspectWidth) {
			ds.setWidth(ds.height() * m_aspectWidth / m_aspectHeight);
		} else if (ds.width() * m_aspectHeight < ds.height() * m_aspectWidth) {
			ds.setHeight(ds.width() * m_aspectHeight / m_aspectWidth);
		}
	}
	if (m_lockIntegerScaling) {
		ds.setWidth(ds.width() - ds.width() % m_aspectWidth);
		ds.setHeight(ds.height() - ds.height() % m_aspectHeight);
	}
	QPoint origin = QPoint((s.width() - ds.width()) / 2, (s.height() - ds.height()) / 2);
	QRect full(origin, ds);
	painter.drawPixmap(full, *logo);
}
