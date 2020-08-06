/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "Window.h"

#include <QKeyEvent>
#include <QKeySequence>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QScreen>
#include <QStackedLayout>
#include <QWindow>

#ifdef USE_SQLITE3
#include "ArchiveInspector.h"
#include "library/LibraryController.h"
#endif

#include "AboutScreen.h"
#include "AudioProcessor.h"
#include "BattleChipView.h"
#include "CheatsView.h"
#include "ConfigController.h"
#include "CoreController.h"
#include "DebuggerConsole.h"
#include "DebuggerConsoleController.h"
#include "Display.h"
#include "CoreController.h"
#include "FrameView.h"
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
#include "VideoProxy.h"
#include "VideoView.h"

#ifdef USE_DISCORD_RPC
#include "DiscordCoordinator.h"
#endif

#include <mgba/core/version.h>
#include <mgba/core/cheats.h>
#ifdef M_CORE_GB
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/video.h>
#endif
#ifdef M_CORE_GBA
#include <mgba/gba/interface.h>
#include <mgba/internal/gba/gba.h>
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
	resizeFrame(QSize(GBA_VIDEO_HORIZONTAL_PIXELS * i, GBA_VIDEO_VERTICAL_PIXELS * i));
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
	connect(&m_focusCheck, &QTimer::timeout, this, &Window::focusCheck);
	connect(&m_inputController, &InputController::profileLoaded, m_shortcutController, &ShortcutController::loadProfile);

	m_log.setLevels(mLOG_WARN | mLOG_ERROR | mLOG_FATAL);
	m_log.load(m_config);
	m_fpsTimer.setInterval(FPS_TIMER_INTERVAL);
	m_focusCheck.setInterval(200);
	m_mustRestart.setInterval(MUST_RESTART_TIMEOUT);
	m_mustRestart.setSingleShot(true);

	m_shortcutController->setConfigController(m_config);
	m_shortcutController->setActionMapper(&m_actions);
	setupMenu(menuBar());
}

Window::~Window() {
	delete m_logView;

#ifdef USE_FFMPEG
	delete m_videoView;
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
	if (windowHandle()) {
		QRect geom = windowHandle()->screen()->availableGeometry();
		if (newSize.width() > geom.width()) {
			newSize.setWidth(geom.width());
		}
		if (newSize.height() > geom.height()) {
			newSize.setHeight(geom.height());
		}
	}
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
	if (m_display) {
		m_display->lockAspectRatio(opts->lockAspectRatio);
		m_display->filter(opts->resampleVideo);
	}
	m_screenWidget->filter(opts->resampleVideo);

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

void Window::selectState(bool load) {
	QStringList formats{"*.ss0", "*.ss1", "*.ss2", "*.ss3", "*.ss4", "*.ss5", "*.ss6", "*.ss7", "*.ss8", "*.ss9"};
	QString filter = tr("mGBA savestate files (%1)").arg(formats.join(QChar(' ')));
	if (load) {
		QString filename = GBAApp::app()->getOpenFileName(this, tr("Select savestate"), filter);
		if (!filename.isEmpty()) {
			m_controller->loadState(filename);
		}
	} else {
		QString filename = GBAApp::app()->getSaveFileName(this, tr("Select savestate"), filter);
		if (!filename.isEmpty()) {
			m_controller->saveState(filename);
		}
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
	for (Action* action : m_nonMpActions) {
		action->setEnabled(attached < 2);
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
	SettingsView* settingsWindow = new SettingsView(m_config, &m_inputController, m_shortcutController, &m_log);
#if defined(BUILD_GL) || defined(BUILD_GLES2)
	if (m_display->supportsShaders()) {
		settingsWindow->setShaderSelector(m_shaderView.get());
	}
#endif
	connect(settingsWindow, &SettingsView::displayDriverChanged, this, &Window::reloadDisplayDriver);
	connect(settingsWindow, &SettingsView::audioDriverChanged, this, &Window::reloadAudioDriver);
	connect(settingsWindow, &SettingsView::cameraDriverChanged, this, &Window::mustRestart);
	connect(settingsWindow, &SettingsView::cameraChanged, &m_inputController, &InputController::setCamera);
	connect(settingsWindow, &SettingsView::videoRendererChanged, this, &Window::mustRestart);
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
	QSize size(GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);
	if (m_controller) {
		size = m_controller->screenDimensions();
	}
	if (m_screenWidget->width() % size.width() == 0 && m_screenWidget->height() % size.height() == 0 &&
	    m_screenWidget->width() / size.width() == m_screenWidget->height() / size.height()) {
		factor = m_screenWidget->width() / size.width();
	}
	m_savedScale = factor;
	for (QMap<int, Action*>::iterator iter = m_frameSizes.begin(); iter != m_frameSizes.end(); ++iter) {
		iter.value()->setActive(iter.key() == factor);
	}

	m_config->setOption("fullscreen", isFullScreen());
}

void Window::showEvent(QShowEvent* event) {
	if (m_wasOpened) {
		if (event->spontaneous() && m_config->getOption("pauseOnMinimize").toInt() && m_controller) {
			focusCheck();
			if (m_autoresume) {
				m_controller->setPaused(false);
				m_autoresume = false;
			}
		}
		return;
	}
	m_wasOpened = true;
	resizeFrame(m_screenWidget->sizeHint());
	QVariant windowPos = m_config->getQtOption("windowPos");
	QRect geom = windowHandle()->screen()->availableGeometry();
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
	reloadDisplayDriver();
	setFocus();
}

void Window::hideEvent(QHideEvent* event) {
	if (!event->spontaneous()) {
		return;
	}
	if (!m_config->getOption("pauseOnMinimize").toInt() || !m_controller) {
		return;
	}
	if (!m_controller->isPaused()) {
		m_autoresume = true;
		m_controller->setPaused(true);
	}
}

void Window::closeEvent(QCloseEvent* event) {
	emit shutdown();
	m_config->setQtOption("windowPos", pos());

	if (m_savedScale > 0) {
		m_config->setOption("height", GBA_VIDEO_VERTICAL_PIXELS * m_savedScale);
		m_config->setOption("width", GBA_VIDEO_HORIZONTAL_PIXELS * m_savedScale);
	}
	saveConfig();
	if (m_controller) {
		event->ignore();
		m_pendingClose = true;
	} else {
		m_display.reset();
	}
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
	for (Action* action : m_gameActions) {
		action->setEnabled(true);
	}
	for (auto action = m_platformActions.begin(); action != m_platformActions.end(); ++action) {
		action.value()->setEnabled(m_controller->platform() == action.key());
	}
	QSize size = m_controller->screenDimensions();
	m_screenWidget->setDimensions(size.width(), size.height());
	m_config->updateOption("lockIntegerScaling");
	m_config->updateOption("lockAspectRatio");
	m_config->updateOption("interframeBlending");
	m_config->updateOption("showOSD");
	if (m_savedScale > 0) {
		resizeFrame(size * m_savedScale);
	}
	attachWidget(m_display.get());
	setFocus();

#ifndef Q_OS_MAC
	if (isFullScreen()) {
		menuBar()->hide();
	}
#endif

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
	m_actions.clearMenu("videoLayers");
	m_actions.clearMenu("audioChannels");
	const mCoreChannelInfo* videoLayers;
	const mCoreChannelInfo* audioChannels;
	size_t nVideo = core->listVideoLayers(core, &videoLayers);
	size_t nAudio = core->listAudioChannels(core, &audioChannels);

	if (nVideo) {
		for (size_t i = 0; i < nVideo; ++i) {
			Action* action = m_actions.addBooleanAction(videoLayers[i].visibleName, QString("videoLayer.%1").arg(videoLayers[i].internalName), [this, videoLayers, i](bool enable) {
				m_controller->thread()->core->enableVideoLayer(m_controller->thread()->core, videoLayers[i].id, enable);
			}, "videoLayers");
			action->setActive(true);
		}
	}
	if (nAudio) {
		for (size_t i = 0; i < nAudio; ++i) {
			Action* action = m_actions.addBooleanAction(audioChannels[i].visibleName, QString("audioChannel.%1").arg(audioChannels[i].internalName), [this, audioChannels, i](bool enable) {
				m_controller->thread()->core->enableAudioChannel(m_controller->thread()->core, audioChannels[i].id, enable);
			}, "audioChannels");
			action->setActive(true);
		}
	}
	m_actions.rebuildMenu(menuBar(), this, *m_shortcutController);

#ifdef USE_DISCORD_RPC
	DiscordCoordinator::gameStarted(m_controller);
#endif
}

void Window::gameStopped() {
	for (Action* action : m_platformActions) {
		action->setEnabled(true);
	}
	for (Action* action : m_gameActions) {
		action->setEnabled(false);
	}
	setWindowFilePath(QString());
	detachWidget(m_display.get());
	m_screenWidget->setDimensions(m_logo.width(), m_logo.height());
	m_screenWidget->setLockIntegerScaling(false);
	m_screenWidget->setLockAspectRatio(true);
	m_screenWidget->setPixmap(m_logo);
	m_screenWidget->unsetCursor();
	if (m_display) {
#ifdef M_CORE_GB
		m_display->setMinimumSize(GB_VIDEO_HORIZONTAL_PIXELS, GB_VIDEO_VERTICAL_PIXELS);
#elif defined(M_CORE_GBA)
		m_display->setMinimumSize(GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);
#endif
	}

	m_actions.clearMenu("videoLayers");
	m_actions.clearMenu("audioChannels");

	m_fpsTimer.stop();
	m_focusCheck.stop();

	if (m_audioProcessor) {
		m_audioProcessor->stop();
		m_audioProcessor.reset();
	}
	m_display->stopDrawing();

	m_controller.reset();
	updateTitle();

	m_display->setVideoProxy({});
	if (m_pendingClose) {
		m_display.reset();
		close();
	}
#ifndef Q_OS_MAC
	menuBar()->show();
#endif

#ifdef USE_DISCORD_RPC
	DiscordCoordinator::gameStopped();
#endif

	emit paused(false);
}

void Window::gameCrashed(const QString& errorMessage) {
	QMessageBox* crash = new QMessageBox(QMessageBox::Critical, tr("Crash"),
	                                     tr("The game has crashed with the following error:\n\n%1").arg(errorMessage),
	                                     QMessageBox::Ok, this, Qt::Sheet);
	crash->setAttribute(Qt::WA_DeleteOnClose);
	crash->show();
}

void Window::gameFailed() {
	QMessageBox* fail = new QMessageBox(QMessageBox::Warning, tr("Couldn't Start"),
	                                    tr("Could not start game."),
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
		if (m_controller->hardwareAccelerated()) {
			mustRestart();
			return;
		}
		m_display->stopDrawing();
		detachWidget(m_display.get());
	}
	m_display = std::move(std::unique_ptr<Display>(Display::create(this)));
#if defined(BUILD_GL) || defined(BUILD_GLES2)
	m_shaderView.reset();
	m_shaderView = std::make_unique<ShaderSelector>(m_display.get(), m_config);
#endif

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
	m_display->lockIntegerScaling(opts->lockIntegerScaling);
	m_display->interframeBlending(opts->interframeBlending);
	m_display->filter(opts->resampleVideo);
	m_screenWidget->filter(opts->resampleVideo);
	m_config->updateOption("showOSD");
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
		connect(m_controller.get(), &CoreController::stateLoaded, m_display.get(), &Display::resizeContext);
		connect(m_controller.get(), &CoreController::stateLoaded, m_display.get(), &Display::forceDraw);
		connect(m_controller.get(), &CoreController::rewound, m_display.get(), &Display::forceDraw);
		connect(m_controller.get(), &CoreController::paused, m_display.get(), &Display::pauseDrawing);
		connect(m_controller.get(), &CoreController::unpaused, m_display.get(), &Display::unpauseDrawing);
		connect(m_controller.get(), &CoreController::frameAvailable, m_display.get(), &Display::framePosted);
		connect(m_controller.get(), &CoreController::statusPosted, m_display.get(), &Display::showMessage);
		connect(m_controller.get(), &CoreController::didReset, m_display.get(), &Display::resizeContext);

		attachWidget(m_display.get());
		m_display->startDrawing(m_controller);
	}
#ifdef M_CORE_GB
	m_display->setMinimumSize(GB_VIDEO_HORIZONTAL_PIXELS, GB_VIDEO_VERTICAL_PIXELS);
#elif defined(M_CORE_GBA)
	m_display->setMinimumSize(GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);
#endif
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
	connect(m_controller.get(), &CoreController::fastForwardChanged, m_audioProcessor.get(), &AudioProcessor::inputParametersChanged);
	connect(m_controller.get(), &CoreController::paused, m_audioProcessor.get(), &AudioProcessor::pause);
	connect(m_controller.get(), &CoreController::unpaused, m_audioProcessor.get(), &AudioProcessor::start);
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
	if (m_mustRestart.isActive()) {
		return;
	}
	m_mustRestart.start();
	QMessageBox* dialog = new QMessageBox(QMessageBox::Warning, tr("Restart needed"),
	                                      tr("Some changes will not take effect until the emulator is restarted."),
	                                      QMessageBox::Ok, this, Qt::Sheet);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->show();
}

void Window::recordFrame() {
	m_frameList.append(m_frameTimer.nsecsElapsed());
	m_frameTimer.restart();
}

void Window::showFPS() {
	if (m_frameList.isEmpty()) {
		updateTitle();
		return;
	}
	qint64 total = 0;
	for (qint64 t : m_frameList) {
		total += t;
	}
	double fps = (m_frameList.size() * 1e10) / total;
	m_frameList.clear();
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
		mCore* core = m_controller->thread()->core;
		core->checksum(m_controller->thread()->core, &crc32, CHECKSUM_CRC32);
		QString filePath = windowFilePath();

		if (m_config->getOption("showFilename").toInt() && !filePath.isNull()) {
			QFileInfo fileInfo(filePath);
			title = fileInfo.fileName();
		} else {
			char gameTitle[17] = { '\0' };
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
			title += tr(" -  Player %1 of %2").arg(multiplayer->playerId(m_controller.get()) + 1).arg(multiplayer->attached());
			for (Action* action : m_nonMpActions) {
				action->setEnabled(false);
			}
		} else {
			for (Action* action : m_nonMpActions) {
				action->setEnabled(true);
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
#ifndef Q_OS_MAC
	menuBar()->show();
#endif
	attachWidget(m_stateWindow);
}

void Window::setupMenu(QMenuBar* menubar) {
	installEventFilter(m_shortcutController);

	menubar->clear();
	m_actions.addMenu(tr("&File"), "file");

	m_actions.addAction(tr("Load &ROM..."), "loadROM", this, &Window::selectROM, "file", QKeySequence::Open);

#ifdef USE_SQLITE3
	m_actions.addAction(tr("Load ROM in archive..."), "loadROMInArchive", this, &Window::selectROMInArchive, "file");
	m_actions.addAction(tr("Add folder to library..."), "addDirToLibrary", this, &Window::addDirToLibrary, "file");
#endif

	addGameAction(tr("Load alternate save..."), "loadAlternateSave", [this]() {
		this->selectSave(false);
	}, "file");
	addGameAction(tr("Load temporary save..."), "loadTemporarySave", [this]() {
		this->selectSave(true);
	}, "file");

	m_actions.addAction(tr("Load &patch..."), "loadPatch", this, &Window::selectPatch, "file");

#ifdef M_CORE_GBA
	Action* bootBIOS = m_actions.addAction(tr("Boot BIOS"), "bootBIOS", [this]() {
		setController(m_manager->loadBIOS(PLATFORM_GBA, m_config->getOption("gba.bios")), QString());
	}, "file");
#endif

	addGameAction(tr("Replace ROM..."), "replaceROM", this, &Window::replaceROM, "file");

	Action* romInfo = addGameAction(tr("ROM &info..."), "romInfo", openControllerTView<ROMInfo>(), "file");

	m_actions.addMenu(tr("Recent"), "mru", "file");
	m_actions.addSeparator("file");

	m_actions.addAction(tr("Make portable"), "makePortable", this, &Window::tryMakePortable, "file");
	m_actions.addSeparator("file");

	Action* loadState = addGameAction(tr("&Load state"), "loadState", [this]() {
		this->openStateWindow(LoadSave::LOAD);
	}, "file", QKeySequence("F10"));
	m_nonMpActions.append(loadState);

	Action* loadStateFile = addGameAction(tr("Load state file..."), "loadStateFile", [this]() {
		this->selectState(true);
	}, "file");
	m_nonMpActions.append(loadStateFile);

	Action* saveState = addGameAction(tr("&Save state"), "saveState", [this]() {
		this->openStateWindow(LoadSave::SAVE);
	}, "file", QKeySequence("Shift+F10"));
	m_nonMpActions.append(saveState);

	Action* saveStateFile = addGameAction(tr("Save state file..."), "saveStateFile", [this]() {
		this->selectState(false);
	}, "file");
	m_nonMpActions.append(saveStateFile);

	m_actions.addMenu(tr("Quick load"), "quickLoad", "file");
	m_actions.addMenu(tr("Quick save"), "quickSave", "file");

	Action* quickLoad = addGameAction(tr("Load recent"), "quickLoad", [this] {
		m_controller->loadState();
	}, "quickLoad");
	m_nonMpActions.append(quickLoad);

	Action* quickSave = addGameAction(tr("Save recent"), "quickSave", [this] {
		m_controller->saveState();
	}, "quickSave");
	m_nonMpActions.append(quickSave);

	m_actions.addSeparator("quickLoad");
	m_actions.addSeparator("quickSave");

	Action* undoLoadState = addGameAction(tr("Undo load state"), "undoLoadState", [this]() {
		m_controller->loadBackupState();
	}, "quickLoad", QKeySequence("F11"));
	m_nonMpActions.append(undoLoadState);

	Action* undoSaveState = addGameAction(tr("Undo save state"), "undoSaveState", [this]() {
		m_controller->saveBackupState();
	}, "quickSave", QKeySequence("Shift+F11"));
	m_nonMpActions.append(undoSaveState);

	m_actions.addSeparator("quickLoad");
	m_actions.addSeparator("quickSave");

	for (int i = 1; i < 10; ++i) {
		Action* quickLoad = addGameAction(tr("State &%1").arg(i),  QString("quickLoad.%1").arg(i), [this, i]() {
			m_controller->loadState(i);
		}, "quickLoad", QString("F%1").arg(i));
		m_nonMpActions.append(quickLoad);

		Action* quickSave = addGameAction(tr("State &%1").arg(i),  QString("quickSave.%1").arg(i), [this, i]() {
			m_controller->saveState(i);
		}, "quickSave", QString("Shift+F%1").arg(i));
		m_nonMpActions.append(quickSave);
	}

	m_actions.addSeparator("file");
	m_actions.addAction(tr("Load camera image..."), "loadCamImage", this, &Window::loadCamImage, "file");

#ifdef M_CORE_GBA
	m_actions.addSeparator("file");
	Action* importShark = addGameAction(tr("Import GameShark Save"), "importShark", this, &Window::importSharkport, "file");
	m_platformActions.insert(PLATFORM_GBA, importShark);

	Action* exportShark = addGameAction(tr("Export GameShark Save"), "exportShark", this, &Window::exportSharkport, "file");
	m_platformActions.insert(PLATFORM_GBA, exportShark);
#endif

	m_actions.addSeparator("file");
	m_multiWindow = m_actions.addAction(tr("New multiplayer window"), "multiWindow", [this]() {
		GBAApp::app()->newWindow();
	}, "file");

#ifndef Q_OS_MAC
	m_actions.addSeparator("file");
#endif

	m_actions.addAction(tr("About..."), "about", openTView<AboutScreen>(), "file");

#ifndef Q_OS_MAC
	m_actions.addAction(tr("E&xit"), "quit", static_cast<QWidget*>(this), &QWidget::close, "file", QKeySequence::Quit);
#endif

	m_actions.addMenu(tr("&Emulation"), "emu");
	addGameAction(tr("&Reset"), "reset", [this]() {
		m_controller->reset();
	}, "emu", QKeySequence("Ctrl+R"));

	addGameAction(tr("Sh&utdown"), "shutdown", [this]() {
		m_controller->stop();
	}, "emu");

	Action* yank = addGameAction(tr("Yank game pak"), "yank", [this]() {
		m_controller->yankPak();
	}, "emu");

	m_actions.addSeparator("emu");

	Action* pause = m_actions.addBooleanAction(tr("&Pause"), "pause", [this](bool paused) {
		if (m_controller) {
			m_controller->setPaused(paused);
		} else {
			m_pendingPause = paused;
		}
	}, "emu", QKeySequence("Ctrl+P"));
	connect(this, &Window::paused, pause, &Action::setActive);

	addGameAction(tr("&Next frame"), "frameAdvance", [this]() {
		m_controller->frameAdvance();
	}, "emu", QKeySequence("Ctrl+N"));

	m_actions.addSeparator("emu");

	m_actions.addHeldAction(tr("Fast forward (held)"), "holdFastForward", [this](bool held) {
		if (m_controller) {
			m_controller->setFastForward(held);
		}
	}, "emu", QKeySequence(Qt::Key_Tab));

	addGameAction(tr("&Fast forward"), "fastForward", [this](bool value) {
		m_controller->forceFastForward(value);
	}, "emu", QKeySequence("Shift+Tab"));

	m_actions.addMenu(tr("Fast forward speed"), "fastForwardSpeed", "emu");
	ConfigOption* ffspeed = m_config->addOption("fastForwardRatio");
	ffspeed->connect([this](const QVariant& value) {
		reloadConfig();
	}, this);
	ffspeed->addValue(tr("Unbounded"), -1.0f, &m_actions, "fastForwardSpeed");
	ffspeed->setValue(QVariant(-1.0f));
	m_actions.addSeparator("fastForwardSpeed");
	for (int i = 2; i < 11; ++i) {
		ffspeed->addValue(tr("%0x").arg(i), i, &m_actions, "fastForwardSpeed");
	}
	m_config->updateOption("fastForwardRatio");

	Action* rewindHeld = m_actions.addHeldAction(tr("Rewind (held)"), "holdRewind", [this](bool held) {
		if (m_controller) {
			m_controller->setRewinding(held);
		}
	}, "emu", QKeySequence("`"));
	m_nonMpActions.append(rewindHeld);

	Action* rewind = addGameAction(tr("Re&wind"), "rewind", [this]() {
		m_controller->rewind();
	}, "emu", QKeySequence("~"));
	m_nonMpActions.append(rewind);

	Action* frameRewind = addGameAction(tr("Step backwards"), "frameRewind", [this] () {
		m_controller->rewind(1);
	}, "emu", QKeySequence("Ctrl+B"));
	m_nonMpActions.append(frameRewind);

	ConfigOption* videoSync = m_config->addOption("videoSync");
	videoSync->addBoolean(tr("Sync to &video"), &m_actions, "emu");
	videoSync->connect([this](const QVariant& value) {
		reloadConfig();
	}, this);
	m_config->updateOption("videoSync");

	ConfigOption* audioSync = m_config->addOption("audioSync");
	audioSync->addBoolean(tr("Sync to &audio"), &m_actions, "emu");
	audioSync->connect([this](const QVariant& value) {
		reloadConfig();
	}, this);
	m_config->updateOption("audioSync");

	m_actions.addSeparator("emu");

	m_actions.addMenu(tr("Solar sensor"), "solar", "emu");
	m_actions.addAction(tr("Increase solar level"), "increaseLuminanceLevel", &m_inputController, &InputController::increaseLuminanceLevel, "solar");
	m_actions.addAction(tr("Decrease solar level"), "decreaseLuminanceLevel", &m_inputController, &InputController::decreaseLuminanceLevel, "solar");
	m_actions.addAction(tr("Brightest solar level"), "maxLuminanceLevel", [this]() {
		m_inputController.setLuminanceLevel(10);
	}, "solar");
	m_actions.addAction(tr("Darkest solar level"), "minLuminanceLevel", [this]() {
		m_inputController.setLuminanceLevel(0);
	}, "solar");

	m_actions.addSeparator("solar");
	for (int i = 0; i <= 10; ++i) {
		m_actions.addAction(tr("Brightness %1").arg(QString::number(i)), QString("luminanceLevel.%1").arg(QString::number(i)), [this, i]() {
			m_inputController.setLuminanceLevel(i);
		}, "solar");
	}

#ifdef M_CORE_GB
	Action* gbPrint = addGameAction(tr("Game Boy Printer..."), "gbPrint", [this]() {
		PrinterView* view = new PrinterView(m_controller);
		openView(view);
		m_controller->attachPrinter();
	}, "emu");
	m_platformActions.insert(PLATFORM_GB, gbPrint);
#endif

#ifdef M_CORE_GBA
	Action* bcGate = addGameAction(tr("BattleChip Gate..."), "bcGate", openControllerTView<BattleChipView>(this), "emu");
	m_platformActions.insert(PLATFORM_GBA, bcGate);
#endif

	m_actions.addMenu(tr("Audio/&Video"), "av");
	m_actions.addMenu(tr("Frame size"), "frame", "av");
	for (int i = 1; i <= 8; ++i) {
		Action* setSize = m_actions.addAction(tr("%1×").arg(QString::number(i)), QString("frame.%1x").arg(QString::number(i)), [this, i]() {
			Action* setSize = m_frameSizes[i];
			showNormal();
			QSize size(GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);
			if (m_controller) {
				size = m_controller->screenDimensions();
			}
			size *= i;
			m_savedScale = i;
			m_config->setOption("scaleMultiplier", i); // TODO: Port to other
			resizeFrame(size);
			setSize->setActive(true);
		}, "frame");
		setSize->setExclusive(true);
		if (m_savedScale == i) {
			setSize->setActive(true);
		}
		m_frameSizes[i] = setSize;
	}
	QKeySequence fullscreenKeys;
#ifdef Q_OS_WIN
	fullscreenKeys = QKeySequence("Alt+Return");
#else
	fullscreenKeys = QKeySequence("Ctrl+F");
#endif
	m_actions.addAction(tr("Toggle fullscreen"), "fullscreen", this, &Window::toggleFullScreen, "frame", fullscreenKeys);

	ConfigOption* lockAspectRatio = m_config->addOption("lockAspectRatio");
	lockAspectRatio->addBoolean(tr("Lock aspect ratio"), &m_actions, "av");
	lockAspectRatio->connect([this](const QVariant& value) {
		if (m_display) {
			m_display->lockAspectRatio(value.toBool());
		}
		if (m_controller) {
			m_screenWidget->setLockAspectRatio(value.toBool());
		}
	}, this);
	m_config->updateOption("lockAspectRatio");

	ConfigOption* lockIntegerScaling = m_config->addOption("lockIntegerScaling");
	lockIntegerScaling->addBoolean(tr("Force integer scaling"), &m_actions, "av");
	lockIntegerScaling->connect([this](const QVariant& value) {
		if (m_display) {
			m_display->lockIntegerScaling(value.toBool());
		}
		if (m_controller) {
			m_screenWidget->setLockIntegerScaling(value.toBool());
		}
	}, this);
	m_config->updateOption("lockIntegerScaling");

	ConfigOption* interframeBlending = m_config->addOption("interframeBlending");
	interframeBlending->addBoolean(tr("Interframe blending"), &m_actions, "av");
	interframeBlending->connect([this](const QVariant& value) {
		if (m_display) {
			m_display->interframeBlending(value.toBool());
		}
	}, this);
	m_config->updateOption("interframeBlending");

	ConfigOption* resampleVideo = m_config->addOption("resampleVideo");
	resampleVideo->addBoolean(tr("Bilinear filtering"), &m_actions, "av");
	resampleVideo->connect([this](const QVariant& value) {
		if (m_display) {
			m_display->filter(value.toBool());
		}
		m_screenWidget->filter(value.toBool());
	}, this);
	m_config->updateOption("resampleVideo");

	m_actions.addMenu(tr("Frame&skip"),"skip", "av");
	ConfigOption* skip = m_config->addOption("frameskip");
	skip->connect([this](const QVariant& value) {
		reloadConfig();
	}, this);
	for (int i = 0; i <= 10; ++i) {
		skip->addValue(QString::number(i), i, &m_actions, "skip");
	}
	m_config->updateOption("frameskip");

	m_actions.addSeparator("av");

	ConfigOption* mute = m_config->addOption("mute");
	mute->addBoolean(tr("Mute"), &m_actions, "av");
	mute->connect([this](const QVariant& value) {
		if (value.toInt()) {
			m_config->setOption("fastForwardMute", true);
		}
		reloadConfig();
	}, this);
	m_config->updateOption("mute");

	m_actions.addMenu(tr("FPS target"),"target", "av");
	ConfigOption* fpsTargetOption = m_config->addOption("fpsTarget");
	QMap<double, Action*> fpsTargets;
	for (int fps : {15, 30, 45, 60, 90, 120, 240}) {
		fpsTargets[fps] = fpsTargetOption->addValue(QString::number(fps), fps, &m_actions, "target");
	}
	m_actions.addSeparator("target");
	double nativeGB = double(GBA_ARM7TDMI_FREQUENCY) / double(VIDEO_TOTAL_LENGTH);
	fpsTargets[nativeGB] = fpsTargetOption->addValue(tr("Native (59.7275)"), nativeGB, &m_actions, "target");

	fpsTargetOption->connect([this, fpsTargets](const QVariant& value) {
		reloadConfig();
		for (auto iter = fpsTargets.begin(); iter != fpsTargets.end(); ++iter) {
			bool enableSignals = iter.value()->blockSignals(true);
			iter.value()->setActive(abs(iter.key() - value.toDouble()) < 0.001);
			iter.value()->blockSignals(enableSignals);
		}
	}, this);
	m_config->updateOption("fpsTarget");

	m_actions.addSeparator("av");

#ifdef USE_PNG
	addGameAction(tr("Take &screenshot"), "screenshot", [this]() {
		m_controller->screenshot();
	}, "av", tr("F12"));
#endif

#ifdef USE_FFMPEG
	addGameAction(tr("Record A/V..."), "recordOutput", this, &Window::openVideoWindow, "av");
	addGameAction(tr("Record GIF..."), "recordGIF", this, &Window::openGIFWindow, "av");
#endif

	m_actions.addSeparator("av");
	m_actions.addMenu(tr("Video layers"), "videoLayers", "av");
	m_actions.addMenu(tr("Audio channels"), "audioChannels", "av");

	addGameAction(tr("Adjust layer placement..."), "placementControl", openControllerTView<PlacementControl>(), "av");

	m_actions.addMenu(tr("&Tools"), "tools");
	m_actions.addAction(tr("View &logs..."), "viewLogs", static_cast<QWidget*>(m_logView), &QWidget::show, "tools");

	m_actions.addAction(tr("Game &overrides..."), "overrideWindow", [this]() {
		if (!m_overrideView) {
			m_overrideView = std::move(std::make_unique<OverrideView>(m_config));
			if (m_controller) {
				m_overrideView->setController(m_controller);
			}
			connect(this, &Window::shutdown, m_overrideView.get(), &QWidget::close);
		}
		m_overrideView->show();
		m_overrideView->recheck();
	}, "tools");

	m_actions.addAction(tr("Game Pak sensors..."), "sensorWindow", [this]() {
		if (!m_sensorView) {
			m_sensorView = std::move(std::make_unique<SensorView>(&m_inputController));
			if (m_controller) {
				m_sensorView->setController(m_controller);
			}
			connect(this, &Window::shutdown, m_sensorView.get(), &QWidget::close);
		}
		m_sensorView->show();
	}, "tools");

	addGameAction(tr("&Cheats..."), "cheatsWindow", openControllerTView<CheatsView>(), "tools");

	m_actions.addSeparator("tools");
	m_actions.addAction(tr("Settings..."), "settings", this, &Window::openSettingsWindow, "tools");

#ifdef USE_DEBUGGERS
	m_actions.addSeparator("tools");
	m_actions.addAction(tr("Open debugger console..."), "debuggerWindow", this, &Window::consoleOpen, "tools");
#ifdef USE_GDB_STUB
	Action* gdbWindow = addGameAction(tr("Start &GDB server..."), "gdbWindow", this, &Window::gdbOpen, "tools");
	m_platformActions.insert(PLATFORM_GBA, gdbWindow);
#endif
#endif
	m_actions.addSeparator("tools");

	addGameAction(tr("View &palette..."), "paletteWindow", openControllerTView<PaletteView>(), "tools");
	addGameAction(tr("View &sprites..."), "spriteWindow", openControllerTView<ObjView>(), "tools");
	addGameAction(tr("View &tiles..."), "tileWindow", openControllerTView<TileView>(), "tools");
	addGameAction(tr("View &map..."), "mapWindow", openControllerTView<MapView>(), "tools");

#ifdef M_CORE_GBA
	Action* frameWindow = addGameAction(tr("&Frame inspector..."), "frameWindow", [this]() {
		if (!m_frameView) {
			m_frameView = new FrameView(m_controller);
			connect(this, &Window::shutdown, this, [this]() {
				if (m_frameView) {
					m_frameView->close();
				}
			});
			connect(m_frameView, &QObject::destroyed, this, [this]() {
				m_frameView = nullptr;
			});
			m_frameView->setAttribute(Qt::WA_DeleteOnClose);
		}
		m_frameView->show();
	}, "tools");
	m_platformActions.insert(PLATFORM_GBA, frameWindow);
#endif

	addGameAction(tr("View memory..."), "memoryView", openControllerTView<MemoryView>(), "tools");
	addGameAction(tr("Search memory..."), "memorySearch", openControllerTView<MemorySearch>(), "tools");

#ifdef M_CORE_GBA
	Action* ioViewer = addGameAction(tr("View &I/O registers..."), "ioViewer", openControllerTView<IOViewer>(), "tools");
	m_platformActions.insert(PLATFORM_GBA, ioViewer);
#endif

	m_actions.addSeparator("tools");
	addGameAction(tr("Record debug video log..."), "recordVL", this, &Window::startVideoLog, "tools");
	addGameAction(tr("Stop debug video log"), "stopVL", [this]() {
		m_controller->endVideoLog();
	}, "tools");

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

	ConfigOption* volumeFf = m_config->addOption("fastForwardVolume");
	volumeFf->connect([this](const QVariant& value) {
		reloadConfig();
	}, this);

	ConfigOption* muteFf = m_config->addOption("fastForwardMute");
	muteFf->connect([this](const QVariant& value) {
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
			updateTitle();
		} else if (m_controller) {
			m_fpsTimer.start();
			m_frameTimer.start();
		}
	}, this);

	ConfigOption* showOSD = m_config->addOption("showOSD");
	showOSD->connect([this](const QVariant& value) {
		if (m_display) {
			m_display->showOSDMessages(value.toBool());
		}
	}, this);

	ConfigOption* videoScale = m_config->addOption("videoScale");
	videoScale->connect([this](const QVariant& value) {
		if (m_display) {
			m_display->setVideoScale(value.toInt());
		}
	}, this);

	m_actions.addHiddenAction(tr("Exit fullscreen"), "exitFullScreen", this, &Window::exitFullScreen, "frame", QKeySequence("Esc"));

	m_actions.addHeldAction(tr("GameShark Button (held)"), "holdGSButton", [this](bool held) {
		if (m_controller) {
			mCheatPressButton(m_controller->cheatDevice(), held);
		}
	}, "tools", QKeySequence(Qt::Key_Apostrophe));

	m_actions.addHiddenMenu(tr("Autofire"), "autofire");
	m_actions.addHeldAction(tr("Autofire A"), "autofireA", [this](bool held) {
		if (m_controller) {
			m_controller->setAutofire(GBA_KEY_A, held);
		}
	}, "autofire");
	m_actions.addHeldAction(tr("Autofire B"), "autofireB", [this](bool held) {
		if (m_controller) {
			m_controller->setAutofire(GBA_KEY_B, held);
		}
	}, "autofire");
	m_actions.addHeldAction(tr("Autofire L"), "autofireL", [this](bool held) {
		if (m_controller) {
			m_controller->setAutofire(GBA_KEY_L, held);
		}
	}, "autofire");
	m_actions.addHeldAction(tr("Autofire R"), "autofireR", [this](bool held) {
		if (m_controller) {
			m_controller->setAutofire(GBA_KEY_R, held);
		}
	}, "autofire");
	m_actions.addHeldAction(tr("Autofire Start"), "autofireStart", [this](bool held) {
		if (m_controller) {
			m_controller->setAutofire(GBA_KEY_START, held);
		}
	}, "autofire");
	m_actions.addHeldAction(tr("Autofire Select"), "autofireSelect", [this](bool held) {
		if (m_controller) {
			m_controller->setAutofire(GBA_KEY_SELECT, held);
		}
	}, "autofire");
	m_actions.addHeldAction(tr("Autofire Up"), "autofireUp", [this](bool held) {
		if (m_controller) {
			m_controller->setAutofire(GBA_KEY_UP, held);
		}
	}, "autofire");
	m_actions.addHeldAction(tr("Autofire Right"), "autofireRight", [this](bool held) {
		if (m_controller) {
			m_controller->setAutofire(GBA_KEY_RIGHT, held);
		}
	}, "autofire");
	m_actions.addHeldAction(tr("Autofire Down"), "autofireDown", [this](bool held) {
		if (m_controller) {
			m_controller->setAutofire(GBA_KEY_DOWN, held);
		}
	}, "autofire");
	m_actions.addHeldAction(tr("Autofire Left"), "autofireLeft", [this](bool held) {
		if (m_controller) {
			m_controller->setAutofire(GBA_KEY_LEFT, held);
		}
	}, "autofire");

	for (Action* action : m_gameActions) {
		action->setEnabled(false);
	}

	m_shortcutController->rebuildItems();
	m_actions.rebuildMenu(menuBar(), this, *m_shortcutController);
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

void Window::clearMRU() {
	m_mruFiles.clear();
	updateMRU();
}

void Window::updateMRU() {
	m_actions.clearMenu("mru");
	int i = 0;
	for (const QString& file : m_mruFiles) {
		QString displayName(QDir::toNativeSeparators(file).replace("&", "&&"));
		m_actions.addAction(displayName, QString("mru.%1").arg(QString::number(i)), [this, file]() {
			setController(m_manager->loadGame(file), file);
		}, "mru", QString("Ctrl+%1").arg(i));
		++i;
	}
	m_config->setMRU(m_mruFiles);
	m_config->write();
	m_actions.addSeparator("mru");
	m_actions.addAction(tr("Clear"), "resetMru", this, &Window::clearMRU, "mru");

	m_actions.rebuildMenu(menuBar(), this, *m_shortcutController);
}

Action* Window::addGameAction(const QString& visibleName, const QString& name, Action::Function function, const QString& menu, const QKeySequence& shortcut) {
	Action* action = m_actions.addAction(visibleName, name, [this, function]() {
		if (m_controller) {
			function();
		}
	}, menu, shortcut);
	m_gameActions.append(action);
	return action;
}

template<typename T, typename V>
Action* Window::addGameAction(const QString& visibleName, const QString& name, T* obj, V (T::*method)(), const QString& menu, const QKeySequence& shortcut) {
	return addGameAction(visibleName, name, [this, obj, method]() {
		if (m_controller) {
			(obj->*method)();
		}
	}, menu, shortcut);
}

Action* Window::addGameAction(const QString& visibleName, const QString& name, Action::BooleanFunction function, const QString& menu, const QKeySequence& shortcut) {
	Action* action = m_actions.addBooleanAction(visibleName, name, [this, function](bool value) {
		if (m_controller) {
			function(value);
		}
	}, menu, shortcut);
	m_gameActions.append(action);
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
	QPixmap pixmap;
	pixmap.convertFromImage(m_controller->getPixels());
	m_screenWidget->setPixmap(pixmap);
	emit paused(true);
}

void Window::setController(CoreController* controller, const QString& fname) {
	if (!controller) {
		return;
	}
	if (m_pendingClose) {
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

	if (!m_display) {
		reloadDisplayDriver();
	}

	if (m_config->getOption("hwaccelVideo").toInt() && m_display->supportsShaders() && controller->supportsFeature(CoreController::Feature::OPENGL)) {
		std::shared_ptr<VideoProxy> proxy = std::make_shared<VideoProxy>();
		m_display->setVideoProxy(proxy);
		proxy->attach(controller);

		int fb = m_display->framebufferHandle();
		if (fb >= 0) {
			controller->setFramebufferHandle(fb);
		}
	}

	m_controller = std::shared_ptr<CoreController>(controller);
	m_inputController.recalibrateAxes();
	m_controller->setInputController(&m_inputController);
	m_controller->setLogger(&m_log);
	m_display->startDrawing(m_controller);

	connect(this, &Window::shutdown, [this]() {
		if (!m_controller) {
			return;
		}
		m_controller->stop();
		disconnect(m_controller.get(), &CoreController::started, this, &Window::gameStarted);
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

	connect(m_controller.get(), &CoreController::stateLoaded, m_display.get(), &Display::resizeContext);
	connect(m_controller.get(), &CoreController::stateLoaded, m_display.get(), &Display::forceDraw);
	connect(m_controller.get(), &CoreController::rewound, m_display.get(), &Display::forceDraw);
	connect(m_controller.get(), &CoreController::paused, m_display.get(), &Display::pauseDrawing);
	connect(m_controller.get(), &CoreController::unpaused, m_display.get(), &Display::unpauseDrawing);
	connect(m_controller.get(), &CoreController::frameAvailable, m_display.get(), &Display::framePosted);
	connect(m_controller.get(), &CoreController::statusPosted, m_display.get(), &Display::showMessage);
	connect(m_controller.get(), &CoreController::didReset, m_display.get(), &Display::resizeContext);

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

#ifdef USE_FFMPEG
	if (m_gifView) {
		m_gifView->setController(m_controller);
	}

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

void WindowBackground::filter(bool filter) {
	m_filter = filter;
}

void WindowBackground::paintEvent(QPaintEvent* event) {
	QWidget::paintEvent(event);
	const QPixmap& logo = pixmap();
	QPainter painter(this);
	painter.setRenderHint(QPainter::SmoothPixmapTransform, m_filter);
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
		if (ds.width() >= m_aspectWidth) {
			ds.setWidth(ds.width() - ds.width() % m_aspectWidth);
		}
		if (ds.height() >= m_aspectHeight) {
			ds.setHeight(ds.height() - ds.height() % m_aspectHeight);
		}
	}
	QPoint origin = QPoint((s.width() - ds.width()) / 2, (s.height() - ds.height()) / 2);
	QRect full(origin, ds);
	painter.drawPixmap(full, logo);
}
