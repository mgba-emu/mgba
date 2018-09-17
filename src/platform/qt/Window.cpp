/* Copyright (c) 2013-2017 Jeffrey Pfau
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
#include "AudioProcessor.h"
#include "CheatsView.h"
#include "ConfigController.h"
#include "CoreController.h"
#include "DebuggerConsole.h"
#include "DebuggerConsoleController.h"
#include "Display.h"
#include "CoreController.h"
#include "GBAApp.h"
#include "GDBController.h"
#include "GDBWindow.h"
#include "GIFView.h"
#include "IOViewer.h"
#include "LoadSaveState.h"
#include "LogView.h"
#include "MapView.h"
#include "MemorySearch.h"
#include "MemoryView.h"
#include "MultiplayerController.h"
#include "OverrideView.h"
#include "ObjView.h"
#include "PaletteView.h"
#include "PlacementControl.h"
#include "PrinterView.h"
#include "ROMInfo.h"
#include "SensorView.h"
#include "SettingsView.h"
#include "ShaderSelector.h"
#include "ShortcutController.h"
#include "TileView.h"
#include "VideoView.h"

#include <mgba/core/version.h>
#include <mgba/core/cheats.h>
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

Window::Window(CoreManager* manager, ConfigController* config, int playerId, QWidget* parent)
	: QMainWindow(parent)
	, m_manager(manager)
	, m_logView(new LogView(&m_log))
	, m_screenWidget(new WindowBackground())
	, m_config(config)
	, m_inputController(playerId, this)
	, m_shortcutController(new ShortcutController(this))
{
	setFocusPolicy(Qt::StrongFocus);
	setAcceptDrops(true);
	setAttribute(Qt::WA_DeleteOnClose);
	updateTitle();
	reloadDisplayDriver();

	m_logo.setDevicePixelRatio(m_screenWidget->devicePixelRatio());
	m_logo = m_logo; // Free memory left over in old pixmap

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
			if (m_controller) {
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
			setController(m_manager->loadGame(output, path.second, path.first), path.first + "/" + path.second);
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

	connect(this, &Window::shutdown, m_logView, &QWidget::hide);
	connect(&m_fpsTimer, &QTimer::timeout, this, &Window::showFPS);
	connect(&m_frameTimer, &QTimer::timeout, this, &Window::delimitFrames);
	connect(&m_focusCheck, &QTimer::timeout, this, &Window::focusCheck);
	connect(&m_inputController, &InputController::profileLoaded, m_shortcutController, &ShortcutController::loadProfile);

	m_log.setLevels(mLOG_WARN | mLOG_ERROR | mLOG_FATAL);
	m_fpsTimer.setInterval(FPS_TIMER_INTERVAL);
	m_frameTimer.setInterval(FRAME_LIST_INTERVAL);
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
		m_pendingPatch = args->patch;
	}

	if (args->savestate) {
		m_pendingState = args->savestate;
	}

	if (args->fname) {
		setController(m_manager->loadGame(args->fname), args->fname);
	}

#ifdef USE_GDB_STUB
	if (args->debuggerType == DEBUGGER_GDB) {
		if (!m_gdbController) {
			m_gdbController = new GDBController(this);
			if (m_controller) {
				m_gdbController->setController(m_controller);
			}
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

	if (opts->width && opts->height) {
		resizeFrame(QSize(opts->width, opts->height));
	}

	if (opts->fullscreen) {
		enterFullScreen();
	}

	m_mruFiles = m_config->getMRU();
	updateMRU();

	m_inputController.setConfiguration(m_config);
}

void Window::reloadConfig() {
	const mCoreOptions* opts = m_config->options();

	m_log.setLevels(opts->logLevel);

	if (m_controller) {
		m_controller->loadConfig(m_config);
		if (m_audioProcessor) {
			m_audioProcessor->setBufferSamples(opts->audioBuffers);
			m_audioProcessor->requestSampleRate(opts->sampleRate);
		}
		m_display->resizeContext();
	}
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
#ifdef USE_ELF
		"*.elf",
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
		"*.sgb",
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
		setController(m_manager->loadGame(filename), filename);
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
			setController(m_manager->loadGame(output, path.second, path.first), path.first + "/" + path.second);
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
	if (!m_controller) {
		return;
	}
	int attached = 1;
	MultiplayerController* multiplayer = m_controller->multiplayerController();
	if (multiplayer) {
		attached = multiplayer->attached();
	}
	for (QAction* action : m_nonMpActions) {
		action->setDisabled(attached > 1);
	}
}

void Window::selectPatch() {
	QString filename = GBAApp::app()->getOpenFileName(this, tr("Select patch"), tr("Patches (*.ips *.ups *.bps)"));
	if (!filename.isEmpty()) {
		if (m_controller) {
			m_controller->loadPatch(filename);
		} else {
			m_pendingPatch = filename;
		}
	}
}

void Window::openView(QWidget* widget) {
	connect(this, &Window::shutdown, widget, &QWidget::close);
	widget->setAttribute(Qt::WA_DeleteOnClose);
	widget->show();
}

void Window::loadCamImage() {
	QString filename = GBAApp::app()->getOpenFileName(this, tr("Select image"), tr("Image file (*.png *.gif *.jpg *.jpeg);;All files (*)"));
	if (!filename.isEmpty()) {
		m_inputController.loadCamImage(filename);
	}
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
		settingsWindow->setShaderSelector(m_shaderView.get());
	}
#endif
	connect(settingsWindow, &SettingsView::displayDriverChanged, this, &Window::reloadDisplayDriver);
	connect(settingsWindow, &SettingsView::audioDriverChanged, this, &Window::reloadAudioDriver);
	connect(settingsWindow, &SettingsView::cameraDriverChanged, this, &Window::mustRestart);
	connect(settingsWindow, &SettingsView::languageChanged, this, &Window::mustRestart);
	connect(settingsWindow, &SettingsView::pathsChanged, this, &Window::reloadConfig);
#ifdef USE_SQLITE3
	connect(settingsWindow, &SettingsView::libraryCleared, m_libraryView, &LibraryController::clear);
#endif
	openView(settingsWindow);
}

void Window::startVideoLog() {
	QString filename = GBAApp::app()->getSaveFileName(this, tr("Select video log"), tr("Video logs (*.mvl)"));
	if (!filename.isEmpty()) {
		m_controller->startVideoLog(filename);
	}
}

template <typename T, typename... A>
std::function<void()> Window::openTView(A... arg) {
	return [=]() {
		T* view = new T(arg...);
		openView(view);
	};
}


template <typename T, typename... A>
std::function<void()> Window::openControllerTView(A... arg) {
	return [=]() {
		T* view = new T(m_controller, arg...);
		openView(view);
	};
}

#ifdef USE_FFMPEG
void Window::openVideoWindow() {
	if (!m_videoView) {
		m_videoView = new VideoView();
		if (m_controller) {
			m_videoView->setController(m_controller);
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
		if (m_controller) {
			m_gifView->setController(m_controller);
		}
		connect(this, &Window::shutdown, m_gifView, &QWidget::close);
	}
	m_gifView->show();
}
#endif

#ifdef USE_GDB_STUB
void Window::gdbOpen() {
	if (!m_gdbController) {
		m_gdbController = new GDBController(this);
	}
	GDBWindow* window = new GDBWindow(m_gdbController);
	m_gdbController->setController(m_controller);
	connect(m_controller.get(), &CoreController::stopping, window, &QWidget::close);
	openView(window);
}
#endif

#ifdef USE_DEBUGGERS
void Window::consoleOpen() {
	if (!m_console) {
		m_console = new DebuggerConsoleController(this);
	}
	DebuggerConsole* window = new DebuggerConsole(m_console);
	if (m_controller) {
		m_console->setController(m_controller);
	}
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
	if (m_controller) {
		m_controller->addKey(key);
	}
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
	if (m_controller) {
		m_controller->clearKey(key);
	}
	event->accept();
}

void Window::resizeEvent(QResizeEvent* event) {
	if (!isFullScreen()) {
		m_config->setOption("height", m_screenWidget->height());
		m_config->setOption("width", m_screenWidget->width());
	}

	int factor = 0;
	QSize size(VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
	if (m_controller) {
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
	QRect geom = QApplication::desktop()->availableGeometry(this);
	if (!windowPos.isNull() && geom.contains(windowPos.toPoint())) {
		move(windowPos.toPoint());
	} else {
		QRect rect = frameGeometry();
		rect.moveCenter(geom.center());
		move(rect.topLeft());
	}
	if (m_fullscreenOnStart) {
		enterFullScreen();
		m_fullscreenOnStart = false;
	}
	if (m_display) {
		reloadDisplayDriver();
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
	setController(m_manager->loadGame(url.toLocalFile()), url.toLocalFile());
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
	if (m_controller && !m_controller->isPaused()) {
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

void Window::gameStarted() {
	for (QAction* action : m_gameActions) {
		action->setDisabled(false);
	}
#ifdef M_CORE_GBA
	for (QAction* action : m_gbaActions) {
		action->setDisabled(m_controller->platform() != PLATFORM_GBA);
	}
#endif
	QSize size = m_controller->screenDimensions();
	m_screenWidget->setDimensions(size.width(), size.height());
	m_config->updateOption("lockIntegerScaling");
	m_config->updateOption("lockAspectRatio");
	if (m_savedScale > 0) {
		resizeFrame(size * m_savedScale);
	}
	attachWidget(m_display.get());
	m_display->setMinimumSize(size);

#ifndef Q_OS_MAC
	if (isFullScreen()) {
		menuBar()->hide();
	}
#endif
	m_display->startDrawing(m_controller);

	reloadAudioDriver();
	multiplayerChanged();
	updateTitle();

	m_hitUnimplementedBiosCall = false;
	if (m_config->getOption("showFps", "1").toInt()) {
		m_fpsTimer.start();
		m_frameTimer.start();
	}
	m_focusCheck.start();
	if (m_display->underMouse()) {
		m_screenWidget->setCursor(Qt::BlankCursor);
	}

	CoreController::Interrupter interrupter(m_controller, true);
	mCore* core = m_controller->thread()->core;
	m_videoLayers->clear();
	m_audioChannels->clear();
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
				m_controller->thread()->core->enableVideoLayer(m_controller->thread()->core, videoLayers[i].id, enable);
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
				m_controller->thread()->core->enableAudioChannel(m_controller->thread()->core, audioChannels[i].id, enable);
			});
			m_audioChannels->addAction(action);
		}
	}
}

void Window::gameStopped() {
	m_controller.reset();
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
	detachWidget(m_display.get());
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

	m_videoLayers->clear();
	m_audioChannels->clear();

	m_fpsTimer.stop();
	m_frameTimer.stop();
	m_focusCheck.stop();

	emit paused(false);
}

void Window::gameCrashed(const QString& errorMessage) {
	QMessageBox* crash = new QMessageBox(QMessageBox::Critical, tr("Crash"),
	                                     tr("The game has crashed with the following error:\n\n%1").arg(errorMessage),
	                                     QMessageBox::Ok, this, Qt::Sheet);
	crash->setAttribute(Qt::WA_DeleteOnClose);
	crash->show();
	m_controller->stop();
}

void Window::gameFailed() {
	QMessageBox* fail = new QMessageBox(QMessageBox::Warning, tr("Couldn't Load"),
	                                    tr("Could not load game. Are you sure it's in the correct format?"),
	                                    QMessageBox::Ok, this, Qt::Sheet);
	fail->setAttribute(Qt::WA_DeleteOnClose);
	fail->show();
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

void Window::reloadDisplayDriver() {
	if (m_controller) {
		m_display->stopDrawing();
		detachWidget(m_display.get());
	}
	m_display = std::move(std::unique_ptr<Display>(Display::create(this)));
#if defined(BUILD_GL) || defined(BUILD_GLES2)
	m_shaderView.reset();
	m_shaderView = std::make_unique<ShaderSelector>(m_display.get(), m_config);
#endif

	connect(this, &Window::shutdown, m_display.get(), &Display::stopDrawing);
	connect(m_display.get(), &Display::hideCursor, [this]() {
		if (static_cast<QStackedLayout*>(m_screenWidget->layout())->currentWidget() == m_display.get()) {
			m_screenWidget->setCursor(Qt::BlankCursor);
		}
	});
	connect(m_display.get(), &Display::showCursor, [this]() {
		m_screenWidget->unsetCursor();
	});

	const mCoreOptions* opts = m_config->options();
	m_display->lockAspectRatio(opts->lockAspectRatio);
	m_display->filter(opts->resampleVideo);
#if defined(BUILD_GL) || defined(BUILD_GLES2)
	if (opts->shader) {
		struct VDir* shader = VDirOpen(opts->shader);
		if (shader && m_display->supportsShaders()) {
			m_display->setShaders(shader);
			m_shaderView->refreshShaders();
			shader->close(shader);
		}
	}
#endif

	if (m_controller) {
		m_display->setMinimumSize(m_controller->screenDimensions());
		connect(m_controller.get(), &CoreController::stopping, m_display.get(), &Display::stopDrawing);
		connect(m_controller.get(), &CoreController::stateLoaded, m_display.get(), &Display::resizeContext);
		connect(m_controller.get(), &CoreController::stateLoaded, m_display.get(), &Display::forceDraw);
		connect(m_controller.get(), &CoreController::rewound, m_display.get(), &Display::forceDraw);
		connect(m_controller.get(), &CoreController::paused, m_display.get(), &Display::pauseDrawing);
		connect(m_controller.get(), &CoreController::unpaused, m_display.get(), &Display::unpauseDrawing);
		connect(m_controller.get(), &CoreController::frameAvailable, m_display.get(), &Display::framePosted);
		connect(m_controller.get(), &CoreController::statusPosted, m_display.get(), &Display::showMessage);

		attachWidget(m_display.get());
		m_display->startDrawing(m_controller);
	} else {
#ifdef M_CORE_GB
		m_display->setMinimumSize(GB_VIDEO_HORIZONTAL_PIXELS, GB_VIDEO_VERTICAL_PIXELS);
#elif defined(M_CORE_GBA)
		m_display->setMinimumSize(VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
#endif
	}
}

void Window::reloadAudioDriver() {
	if (!m_controller) {
		return;
	}
	if (m_audioProcessor) {
		m_audioProcessor->stop();
		m_audioProcessor.reset();
	}

	const mCoreOptions* opts = m_config->options();
	m_audioProcessor = std::move(std::unique_ptr<AudioProcessor>(AudioProcessor::create()));
	m_audioProcessor->setInput(m_controller);
	m_audioProcessor->setBufferSamples(opts->audioBuffers);
	m_audioProcessor->requestSampleRate(opts->sampleRate);
	m_audioProcessor->start();
	connect(m_controller.get(), &CoreController::stopping, m_audioProcessor.get(), &AudioProcessor::stop);
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
	if (m_frameList.isEmpty()) {
		m_frameList.append(1);
	} else {
		++m_frameList.back();
	}
}

void Window::delimitFrames() {
	if (m_frameList.size() >= FRAME_LIST_SIZE) {
		m_frameCounter -= m_frameList.takeAt(0);
	}
	m_frameCounter += m_frameList.back();
	m_frameList.append(0);
}

void Window::showFPS() {
	if (m_frameList.isEmpty()) {
		updateTitle();
		return;
	}
	float fps = m_frameCounter * 10000.f / (FRAME_LIST_INTERVAL * (m_frameList.size() - 1));
	fps = round(fps) / 10.f;
	updateTitle(fps);
}

void Window::updateTitle(float fps) {
	QString title;

	if (m_controller) {
		CoreController::Interrupter interrupter(m_controller);
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
		MultiplayerController* multiplayer = m_controller->multiplayerController();
		if (multiplayer && multiplayer->attached() > 1) {
			title += tr(" -  Player %1 of %2").arg(multiplayer->playerId(m_controller.get()) + 1).arg(multiplayer->attached());
			for (QAction* action : m_nonMpActions) {
				action->setDisabled(true);
			}
		} else {
			for (QAction* action : m_nonMpActions) {
				action->setDisabled(false);
			}
		}
	}
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
	connect(m_stateWindow, &LoadSaveState::closed, [this]() {
		detachWidget(m_stateWindow);
		m_stateWindow = nullptr;
		QMetaObject::invokeMethod(this, "setFocus", Qt::QueuedConnection);
	});
	if (!wasPaused) {
		m_controller->setPaused(true);
		connect(m_stateWindow, &LoadSaveState::closed, [this]() {
			if (m_controller) {
				m_controller->setPaused(false);
			}
		});
	}
	m_stateWindow->setAttribute(Qt::WA_DeleteOnClose);
	m_stateWindow->setMode(ls);
	updateFrame();
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

	QAction* loadAlternateSave = new QAction(tr("Load alternate save..."), fileMenu);
	connect(loadAlternateSave, &QAction::triggered, [this]() { this->selectSave(false); });
	m_gameActions.append(loadAlternateSave);
	addControlledAction(fileMenu, loadAlternateSave, "loadAlternateSave");

	QAction* loadTemporarySave = new QAction(tr("Load temporary save..."), fileMenu);
	connect(loadTemporarySave, &QAction::triggered, [this]() { this->selectSave(true); });
	m_gameActions.append(loadTemporarySave);
	addControlledAction(fileMenu, loadTemporarySave, "loadTemporarySave");

	addControlledAction(fileMenu, fileMenu->addAction(tr("Load &patch..."), this, SLOT(selectPatch())), "loadPatch");

#ifdef M_CORE_GBA
	QAction* bootBIOS = new QAction(tr("Boot BIOS"), fileMenu);
	connect(bootBIOS, &QAction::triggered, [this]() {
		setController(m_manager->loadBIOS(PLATFORM_GBA, m_config->getOption("gba.bios")), QString());
	});
	addControlledAction(fileMenu, bootBIOS, "bootBIOS");
#endif

	addControlledAction(fileMenu, fileMenu->addAction(tr("Replace ROM..."), this, SLOT(replaceROM())), "replaceROM");

	QAction* romInfo = new QAction(tr("ROM &info..."), fileMenu);
	connect(romInfo, &QAction::triggered, openControllerTView<ROMInfo>());
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
	connect(quickLoad, &QAction::triggered, [this] {
		m_controller->loadState();
	});
	m_gameActions.append(quickLoad);
	m_nonMpActions.append(quickLoad);
	addControlledAction(quickLoadMenu, quickLoad, "quickLoad");

	QAction* quickSave = new QAction(tr("Save recent"), quickSaveMenu);
	connect(quickLoad, &QAction::triggered, [this] {
		m_controller->saveState();
	});
	m_gameActions.append(quickSave);
	m_nonMpActions.append(quickSave);
	addControlledAction(quickSaveMenu, quickSave, "quickSave");

	quickLoadMenu->addSeparator();
	quickSaveMenu->addSeparator();

	QAction* undoLoadState = new QAction(tr("Undo load state"), quickLoadMenu);
	undoLoadState->setShortcut(tr("F11"));
	connect(undoLoadState, &QAction::triggered, [this]() {
		m_controller->loadBackupState();
	});
	m_gameActions.append(undoLoadState);
	m_nonMpActions.append(undoLoadState);
	addControlledAction(quickLoadMenu, undoLoadState, "undoLoadState");

	QAction* undoSaveState = new QAction(tr("Undo save state"), quickSaveMenu);
	undoSaveState->setShortcut(tr("Shift+F11"));
	connect(undoSaveState, &QAction::triggered, [this]() {
		m_controller->saveBackupState();
	});
	m_gameActions.append(undoSaveState);
	m_nonMpActions.append(undoSaveState);
	addControlledAction(quickSaveMenu, undoSaveState, "undoSaveState");

	quickLoadMenu->addSeparator();
	quickSaveMenu->addSeparator();

	int i;
	for (i = 1; i < 10; ++i) {
		quickLoad = new QAction(tr("State &%1").arg(i), quickLoadMenu);
		quickLoad->setShortcut(tr("F%1").arg(i));
		connect(quickLoad, &QAction::triggered, [this, i]() {
			m_controller->loadState(i);
		});
		m_gameActions.append(quickLoad);
		m_nonMpActions.append(quickLoad);
		addControlledAction(quickLoadMenu, quickLoad, QString("quickLoad.%1").arg(i));

		quickSave = new QAction(tr("State &%1").arg(i), quickSaveMenu);
		quickSave->setShortcut(tr("Shift+F%1").arg(i));
		connect(quickSave, &QAction::triggered, [this, i]() {
			m_controller->saveState(i);
		});
		m_gameActions.append(quickSave);
		m_nonMpActions.append(quickSave);
		addControlledAction(quickSaveMenu, quickSave, QString("quickSave.%1").arg(i));
	}

	fileMenu->addSeparator();
	QAction* camImage = new QAction(tr("Load camera image..."), fileMenu);
	connect(camImage, &QAction::triggered, this, &Window::loadCamImage);
	addControlledAction(fileMenu, camImage, "loadCamImage");

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
	connect(about, &QAction::triggered, openTView<AboutScreen>());
	fileMenu->addAction(about);

#ifndef Q_OS_MAC
	addControlledAction(fileMenu, fileMenu->addAction(tr("E&xit"), this, SLOT(close()), QKeySequence::Quit), "quit");
#endif

	QMenu* emulationMenu = menubar->addMenu(tr("&Emulation"));
	m_shortcutController->addMenu(emulationMenu);
	QAction* reset = new QAction(tr("&Reset"), emulationMenu);
	reset->setShortcut(tr("Ctrl+R"));
	connect(reset, &QAction::triggered, [this]() {
		m_controller->reset();
	});
	m_gameActions.append(reset);
	addControlledAction(emulationMenu, reset, "reset");

	QAction* shutdown = new QAction(tr("Sh&utdown"), emulationMenu);
	connect(shutdown, &QAction::triggered, [this]() {
		m_controller->stop();
	});
	m_gameActions.append(shutdown);
	addControlledAction(emulationMenu, shutdown, "shutdown");

#ifdef M_CORE_GBA
	QAction* yank = new QAction(tr("Yank game pak"), emulationMenu);
	connect(yank, &QAction::triggered, [this]() {
		m_controller->yankPak();
	});
	m_gameActions.append(yank);
	m_gbaActions.append(yank);
	addControlledAction(emulationMenu, yank, "yank");
#endif
	emulationMenu->addSeparator();

	QAction* pause = new QAction(tr("&Pause"), emulationMenu);
	pause->setChecked(false);
	pause->setCheckable(true);
	pause->setShortcut(tr("Ctrl+P"));
	connect(pause, &QAction::triggered, [this](bool paused) {
		if (m_controller) {
			m_controller->setPaused(paused);
		} else {
			m_pendingPause = paused;
		}
	});
	connect(this, &Window::paused, [pause](bool paused) {
		pause->setChecked(paused);
	});
	addControlledAction(emulationMenu, pause, "pause");

	QAction* frameAdvance = new QAction(tr("&Next frame"), emulationMenu);
	frameAdvance->setShortcut(tr("Ctrl+N"));
	connect(frameAdvance, &QAction::triggered, [this]() {
		m_controller->frameAdvance();
	});
	m_gameActions.append(frameAdvance);
	addControlledAction(emulationMenu, frameAdvance, "frameAdvance");

	emulationMenu->addSeparator();

	m_shortcutController->addFunctions(emulationMenu, [this]() {
		if (m_controller) {
			m_controller->setFastForward(true);
		}
	}, [this]() {
		if (m_controller) {
			m_controller->setFastForward(false);
		}
	}, QKeySequence(Qt::Key_Tab), tr("Fast forward (held)"), "holdFastForward");

	QAction* turbo = new QAction(tr("&Fast forward"), emulationMenu);
	turbo->setCheckable(true);
	turbo->setChecked(false);
	turbo->setShortcut(tr("Shift+Tab"));
	connect(turbo, &QAction::triggered, [this](bool value) {
		m_controller->forceFastForward(value);
	});
	addControlledAction(emulationMenu, turbo, "fastForward");
	m_gameActions.append(turbo);

	QMenu* ffspeedMenu = emulationMenu->addMenu(tr("Fast forward speed"));
	ConfigOption* ffspeed = m_config->addOption("fastForwardRatio");
	ffspeed->connect([this](const QVariant& value) {
		reloadConfig();
	}, this);
	ffspeed->addValue(tr("Unbounded"), -1.0f, ffspeedMenu);
	ffspeed->setValue(QVariant(-1.0f));
	ffspeedMenu->addSeparator();
	for (i = 2; i < 11; ++i) {
		ffspeed->addValue(tr("%0x").arg(i), i, ffspeedMenu);
	}
	m_config->updateOption("fastForwardRatio");

	m_shortcutController->addFunctions(emulationMenu, [this]() {
		if (m_controller) {
			m_controller->setRewinding(true);
		}
	}, [this]() {
		if (m_controller) {
			m_controller->setRewinding(false);
		}
	}, QKeySequence("`"), tr("Rewind (held)"), "holdRewind");

	QAction* rewind = new QAction(tr("Re&wind"), emulationMenu);
	rewind->setShortcut(tr("~"));
	connect(rewind, &QAction::triggered, [this]() {
		m_controller->rewind();
	});
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
		reloadConfig();
	}, this);
	m_config->updateOption("videoSync");

	ConfigOption* audioSync = m_config->addOption("audioSync");
	audioSync->addBoolean(tr("Sync to &audio"), emulationMenu);
	audioSync->connect([this](const QVariant& value) {
		reloadConfig();
	}, this);
	m_config->updateOption("audioSync");

	emulationMenu->addSeparator();

	QMenu* solarMenu = emulationMenu->addMenu(tr("Solar sensor"));
	m_shortcutController->addMenu(solarMenu);
	QAction* solarIncrease = new QAction(tr("Increase solar level"), solarMenu);
	connect(solarIncrease, &QAction::triggered, &m_inputController, &InputController::increaseLuminanceLevel);
	addControlledAction(solarMenu, solarIncrease, "increaseLuminanceLevel");

	QAction* solarDecrease = new QAction(tr("Decrease solar level"), solarMenu);
	connect(solarDecrease, &QAction::triggered, &m_inputController, &InputController::decreaseLuminanceLevel);
	addControlledAction(solarMenu, solarDecrease, "decreaseLuminanceLevel");

	QAction* maxSolar = new QAction(tr("Brightest solar level"), solarMenu);
	connect(maxSolar, &QAction::triggered, [this]() { m_inputController.setLuminanceLevel(10); });
	addControlledAction(solarMenu, maxSolar, "maxLuminanceLevel");

	QAction* minSolar = new QAction(tr("Darkest solar level"), solarMenu);
	connect(minSolar, &QAction::triggered, [this]() { m_inputController.setLuminanceLevel(0); });
	addControlledAction(solarMenu, minSolar, "minLuminanceLevel");

	solarMenu->addSeparator();
	for (int i = 0; i <= 10; ++i) {
		QAction* setSolar = new QAction(tr("Brightness %1").arg(QString::number(i)), solarMenu);
		connect(setSolar, &QAction::triggered, [this, i]() {
			m_inputController.setLuminanceLevel(i);
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
			if (m_controller) {
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
		if (m_controller) {
			m_screenWidget->setLockAspectRatio(value.toBool());
		}
	}, this);
	m_config->updateOption("lockAspectRatio");

	ConfigOption* lockIntegerScaling = m_config->addOption("lockIntegerScaling");
	lockIntegerScaling->addBoolean(tr("Force integer scaling"), avMenu);
	lockIntegerScaling->connect([this](const QVariant& value) {
		m_display->lockIntegerScaling(value.toBool());
		if (m_controller) {
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
	QMap<double, QAction*> fpsTargets;
	for (int fps : {15, 30, 45, 60, 90, 120, 240}) {
		fpsTargets[fps] = fpsTargetOption->addValue(QString::number(fps), fps, target);
	}
	target->addSeparator();
	double nativeGB = double(GBA_ARM7TDMI_FREQUENCY) / double(VIDEO_TOTAL_LENGTH);
	fpsTargets[nativeGB] = fpsTargetOption->addValue(tr("Native (59.7275)"), nativeGB, target);

	fpsTargetOption->connect([this, fpsTargets](const QVariant& value) {
		reloadConfig();
		for (auto iter = fpsTargets.begin(); iter != fpsTargets.end(); ++iter) {
			bool enableSignals = iter.value()->blockSignals(true);
			iter.value()->setChecked(abs(iter.key() - value.toDouble()) < 0.001);
			iter.value()->blockSignals(enableSignals);
		}
	}, this);
	m_config->updateOption("fpsTarget");

	avMenu->addSeparator();

#ifdef USE_PNG
	QAction* screenshot = new QAction(tr("Take &screenshot"), avMenu);
	screenshot->setShortcut(tr("F12"));
	connect(screenshot, &QAction::triggered, [this]() {
		m_controller->screenshot();
	});
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
	connect(stopVL, &QAction::triggered, [this]() {
		m_controller->endVideoLog();
	});
	addControlledAction(avMenu, stopVL, "stopVL");
	m_gameActions.append(stopVL);

#ifdef M_CORE_GB
	QAction* gbPrint = new QAction(tr("Game Boy Printer..."), avMenu);
	connect(gbPrint, &QAction::triggered, [this]() {
		PrinterView* view = new PrinterView(m_controller);
		openView(view);
		m_controller->attachPrinter();

	});
	addControlledAction(avMenu, gbPrint, "gbPrint");
	m_gameActions.append(gbPrint);
#endif

	avMenu->addSeparator();
	m_videoLayers = avMenu->addMenu(tr("Video layers"));
	m_shortcutController->addMenu(m_videoLayers, avMenu);

	m_audioChannels = avMenu->addMenu(tr("Audio channels"));
	m_shortcutController->addMenu(m_audioChannels, avMenu);

	QAction* placementControl = new QAction(tr("Adjust layer placement..."), avMenu);
	connect(placementControl, &QAction::triggered, openControllerTView<PlacementControl>());
	m_gameActions.append(placementControl);
	addControlledAction(avMenu, placementControl, "placementControl");

	QMenu* toolsMenu = menubar->addMenu(tr("&Tools"));
	m_shortcutController->addMenu(toolsMenu);
	QAction* viewLogs = new QAction(tr("View &logs..."), toolsMenu);
	connect(viewLogs, &QAction::triggered, m_logView, &QWidget::show);
	addControlledAction(toolsMenu, viewLogs, "viewLogs");

	QAction* overrides = new QAction(tr("Game &overrides..."), toolsMenu);
	connect(overrides, &QAction::triggered, [this]() {
		if (!m_overrideView) {
			m_overrideView = std::move(std::make_unique<OverrideView>(m_config));
			if (m_controller) {
				m_overrideView->setController(m_controller);
			}
			connect(this, &Window::shutdown, m_overrideView.get(), &QWidget::close);
		}
		m_overrideView->show();
		m_overrideView->recheck();
	});
	addControlledAction(toolsMenu, overrides, "overrideWindow");

	QAction* sensors = new QAction(tr("Game &Pak sensors..."), toolsMenu);
	connect(sensors, &QAction::triggered, [this]() {
		if (!m_sensorView) {
			m_sensorView = std::move(std::make_unique<SensorView>(&m_inputController));
			if (m_controller) {
				m_sensorView->setController(m_controller);
			}
			connect(this, &Window::shutdown, m_sensorView.get(), &QWidget::close);
		}
		m_sensorView->show();
	});
	addControlledAction(toolsMenu, sensors, "sensorWindow");

	QAction* cheats = new QAction(tr("&Cheats..."), toolsMenu);
	connect(cheats, &QAction::triggered, openControllerTView<CheatsView>());
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
	m_gameActions.append(gdbWindow);
	addControlledAction(toolsMenu, gdbWindow, "gdbWindow");
#endif
	toolsMenu->addSeparator();

	QAction* paletteView = new QAction(tr("View &palette..."), toolsMenu);
	connect(paletteView, &QAction::triggered, openControllerTView<PaletteView>());
	m_gameActions.append(paletteView);
	addControlledAction(toolsMenu, paletteView, "paletteWindow");

	QAction* objView = new QAction(tr("View &sprites..."), toolsMenu);
	connect(objView, &QAction::triggered, openControllerTView<ObjView>());
	m_gameActions.append(objView);
	addControlledAction(toolsMenu, objView, "spriteWindow");

	QAction* tileView = new QAction(tr("View &tiles..."), toolsMenu);
	connect(tileView, &QAction::triggered, openControllerTView<TileView>());
	m_gameActions.append(tileView);
	addControlledAction(toolsMenu, tileView, "tileWindow");

	QAction* mapView = new QAction(tr("View &map..."), toolsMenu);
	connect(mapView, &QAction::triggered, openControllerTView<MapView>());
	m_gameActions.append(mapView);
	addControlledAction(toolsMenu, mapView, "mapWindow");

	QAction* memoryView = new QAction(tr("View memory..."), toolsMenu);
	connect(memoryView, &QAction::triggered, openControllerTView<MemoryView>());
	m_gameActions.append(memoryView);
	addControlledAction(toolsMenu, memoryView, "memoryView");

	QAction* memorySearch = new QAction(tr("Search memory..."), toolsMenu);
	connect(memorySearch, &QAction::triggered, openControllerTView<MemorySearch>());
	m_gameActions.append(memorySearch);
	addControlledAction(toolsMenu, memorySearch, "memorySearch");

#ifdef M_CORE_GBA
	QAction* ioViewer = new QAction(tr("View &I/O registers..."), toolsMenu);
	connect(ioViewer, &QAction::triggered, openControllerTView<IOViewer>());
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
		reloadConfig();
	}, this);

	ConfigOption* buffers = m_config->addOption("audioBuffers");
	buffers->connect([this](const QVariant& value) {
		reloadConfig();
	}, this);

	ConfigOption* sampleRate = m_config->addOption("sampleRate");
	sampleRate->connect([this](const QVariant& value) {
		reloadConfig();
	}, this);

	ConfigOption* volume = m_config->addOption("volume");
	volume->connect([this](const QVariant& value) {
		reloadConfig();
	}, this);

	ConfigOption* rewindEnable = m_config->addOption("rewindEnable");
	rewindEnable->connect([this](const QVariant& value) {
		reloadConfig();
	}, this);

	ConfigOption* rewindBufferCapacity = m_config->addOption("rewindBufferCapacity");
	rewindBufferCapacity->connect([this](const QVariant& value) {
		reloadConfig();
	}, this);

	ConfigOption* allowOpposingDirections = m_config->addOption("allowOpposingDirections");
	allowOpposingDirections->connect([this](const QVariant& value) {
		reloadConfig();
	}, this);

	ConfigOption* saveStateExtdata = m_config->addOption("saveStateExtdata");
	saveStateExtdata->connect([this](const QVariant& value) {
		reloadConfig();
	}, this);

	ConfigOption* loadStateExtdata = m_config->addOption("loadStateExtdata");
	loadStateExtdata->connect([this](const QVariant& value) {
		reloadConfig();
	}, this);

	ConfigOption* preload = m_config->addOption("preload");
	preload->connect([this](const QVariant& value) {
		m_manager->setPreload(value.toBool());
	}, this);
	m_config->updateOption("preload");

	ConfigOption* showFps = m_config->addOption("showFps");
	showFps->connect([this](const QVariant& value) {
		if (!value.toInt()) {
			m_fpsTimer.stop();
			m_frameTimer.stop();
			updateTitle();
		} else if (m_controller) {
			m_fpsTimer.start();
			m_frameTimer.start();
		}
	}, this);

	QAction* exitFullScreen = new QAction(tr("Exit fullscreen"), frameMenu);
	connect(exitFullScreen, &QAction::triggered, this, &Window::exitFullScreen);
	exitFullScreen->setShortcut(QKeySequence("Esc"));
	addHiddenAction(frameMenu, exitFullScreen, "exitFullScreen");

	m_shortcutController->addFunctions(toolsMenu, [this]() {
		if (m_controller) {
			mCheatPressButton(m_controller->cheatDevice(), true);
		}
	}, [this]() {
		if (m_controller) {
			mCheatPressButton(m_controller->cheatDevice(), false);
		}
	}, QKeySequence(Qt::Key_Apostrophe), tr("GameShark Button (held)"), "holdGSButton");

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
		connect(item, &QAction::triggered, [this, file]() {
			setController(m_manager->loadGame(file), file);
		});
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
	if (!m_config->getOption("pauseOnFocusLost").toInt() || !m_controller) {
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

void Window::updateFrame() {
	QSize size = m_controller->screenDimensions();
	QImage currentImage(reinterpret_cast<const uchar*>(m_controller->drawContext()), size.width(), size.height(),
	                    256 * BYTES_PER_PIXEL, QImage::Format_RGBX8888);
	QPixmap pixmap;
	pixmap.convertFromImage(currentImage);
	m_screenWidget->setPixmap(pixmap);
	emit paused(true);
}

void Window::setController(CoreController* controller, const QString& fname) {
	if (!controller) {
		return;
	}

	if (m_controller) {
		m_controller->stop();
		QTimer::singleShot(0, this, [this, controller, fname]() {
			setController(controller, fname);
		});
		return;
	}
	if (!fname.isEmpty()) {
		setWindowFilePath(fname);
		appendMRU(fname);
	}

	m_controller = std::shared_ptr<CoreController>(controller);
	m_inputController.recalibrateAxes();
	m_controller->setInputController(&m_inputController);
	m_controller->setLogger(&m_log);

	connect(this, &Window::shutdown, [this]() {
		if (!m_controller) {
			return;
		}
		m_controller->stop();
	});

	connect(m_controller.get(), &CoreController::started, this, &Window::gameStarted);
	connect(m_controller.get(), &CoreController::started, &m_inputController, &InputController::suspendScreensaver);
	connect(m_controller.get(), &CoreController::stopping, this, &Window::gameStopped);
	{
		connect(m_controller.get(), &CoreController::stopping, [this]() {
			m_controller.reset();
		});
	}
	connect(m_controller.get(), &CoreController::stopping, &m_inputController, &InputController::resumeScreensaver);
	connect(m_controller.get(), &CoreController::paused, this, &Window::updateFrame);

#ifndef Q_OS_MAC
	connect(m_controller.get(), &CoreController::paused, menuBar(), &QWidget::show);
	connect(m_controller.get(), &CoreController::unpaused, [this]() {
		if(isFullScreen()) {
			menuBar()->hide();
		}
	});
#endif

	connect(m_controller.get(), &CoreController::paused, &m_inputController, &InputController::resumeScreensaver);
	connect(m_controller.get(), &CoreController::unpaused, [this]() {
		emit paused(false);
	});

	connect(m_controller.get(), &CoreController::stopping, m_display.get(), &Display::stopDrawing);
	connect(m_controller.get(), &CoreController::stateLoaded, m_display.get(), &Display::resizeContext);
	connect(m_controller.get(), &CoreController::stateLoaded, m_display.get(), &Display::forceDraw);
	connect(m_controller.get(), &CoreController::rewound, m_display.get(), &Display::forceDraw);
	connect(m_controller.get(), &CoreController::paused, m_display.get(), &Display::pauseDrawing);
	connect(m_controller.get(), &CoreController::unpaused, m_display.get(), &Display::unpauseDrawing);
	connect(m_controller.get(), &CoreController::frameAvailable, m_display.get(), &Display::framePosted);
	connect(m_controller.get(), &CoreController::statusPosted, m_display.get(), &Display::showMessage);

	connect(m_controller.get(), &CoreController::unpaused, &m_inputController, &InputController::suspendScreensaver);
	connect(m_controller.get(), &CoreController::frameAvailable, this, &Window::recordFrame);
	connect(m_controller.get(), &CoreController::crashed, this, &Window::gameCrashed);
	connect(m_controller.get(), &CoreController::failed, this, &Window::gameFailed);
	connect(m_controller.get(), &CoreController::unimplementedBiosCall, this, &Window::unimplementedBiosCall);

#ifdef USE_GDB_STUB
	if (m_gdbController) {
		m_gdbController->setController(m_controller);
	}
#endif

#ifdef USE_DEBUGGERS
	if (m_console) {
		m_console->setController(m_controller);
	}
#endif

#ifdef USE_MAGICK
	if (m_gifView) {
		m_gifView->setController(m_controller);
	}
#endif

#ifdef USE_FFMPEG
	if (m_videoView) {
		m_videoView->setController(m_controller);
	}
#endif

	if (m_sensorView) {
		m_sensorView->setController(m_controller);
	}

	if (m_overrideView) {
		m_overrideView->setController(m_controller);
	}

	if (!m_pendingPatch.isEmpty()) {
		m_controller->loadPatch(m_pendingPatch);
		m_pendingPatch = QString();
	}

	m_controller->loadConfig(m_config);
	m_controller->start();

	if (!m_pendingState.isEmpty()) {
		m_controller->loadState(m_pendingState);
		m_pendingState = QString();
	}

	if (m_pendingPause) {
		m_controller->setPaused(true);
		m_pendingPause = false;
	}
}

WindowBackground::WindowBackground(QWidget* parent)
	: QWidget(parent)
{
	setLayout(new QStackedLayout());
	layout()->setContentsMargins(0, 0, 0, 0);
}

void WindowBackground::setPixmap(const QPixmap& pmap) {
	m_pixmap = pmap;
	update();
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

void WindowBackground::paintEvent(QPaintEvent* event) {
	QWidget::paintEvent(event);
	const QPixmap& logo = pixmap();
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
	painter.drawPixmap(full, logo);
}
