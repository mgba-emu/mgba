/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "Window.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QStackedLayout>

#include "CheatsView.h"
#include "ConfigController.h"
#include "GameController.h"
#include "GBAKeyEditor.h"
#include "GDBController.h"
#include "GDBWindow.h"
#include "GIFView.h"
#include "LoadSaveState.h"
#include "LogView.h"
#include "MultiplayerController.h"
#include "OverrideView.h"
#include "SensorView.h"
#include "SettingsView.h"
#include "ShortcutController.h"
#include "ShortcutView.h"
#include "VideoView.h"

extern "C" {
#include "platform/commandline.h"
}

using namespace QGBA;

Window::Window(ConfigController* config, int playerId, QWidget* parent)
	: QMainWindow(parent)
	, m_logView(new LogView())
	, m_stateWindow(nullptr)
	, m_screenWidget(new WindowBackground())
	, m_logo(":/res/mgba-1024.png")
	, m_config(config)
	, m_inputController(playerId)
#ifdef USE_FFMPEG
	, m_videoView(nullptr)
#endif
#ifdef USE_MAGICK
	, m_gifView(nullptr)
#endif
#ifdef USE_GDB_STUB
	, m_gdbController(nullptr)
#endif
	, m_mruMenu(nullptr)
	, m_shortcutController(new ShortcutController(this))
	, m_playerId(playerId)
{
	setWindowTitle(projectName);
	setFocusPolicy(Qt::StrongFocus);
	setAcceptDrops(true);
	m_controller = new GameController(this);
	m_controller->setInputController(&m_inputController);
	m_controller->setOverrides(m_config->overrides());

	QGLFormat format(QGLFormat(QGL::Rgba | QGL::DoubleBuffer));
	format.setSwapInterval(1);
	m_display = new Display(format, this);

	m_logo.setDevicePixelRatio(m_screenWidget->devicePixelRatio());
	m_logo = m_logo; // Free memory left over in old pixmap

	m_screenWidget->setMinimumSize(m_display->minimumSize());
	m_screenWidget->setSizePolicy(m_display->sizePolicy());
	m_screenWidget->setSizeHint(m_display->minimumSize() * 2);
	m_screenWidget->setPixmap(m_logo);
	m_screenWidget->setLockAspectRatio(m_logo.width(), m_logo.height());
	setCentralWidget(m_screenWidget);

	QVariant windowPos = m_config->getQtOption("windowPos");
	if (!windowPos.isNull()) {
		move(windowPos.toPoint());
	}

	connect(m_controller, SIGNAL(gameStarted(GBAThread*)), this, SLOT(gameStarted(GBAThread*)));
	connect(m_controller, SIGNAL(gameStopped(GBAThread*)), m_display, SLOT(stopDrawing()));
	connect(m_controller, SIGNAL(gameStopped(GBAThread*)), this, SLOT(gameStopped()));
	connect(m_controller, SIGNAL(stateLoaded(GBAThread*)), m_display, SLOT(forceDraw()));
	connect(m_controller, SIGNAL(gamePaused(GBAThread*)), m_display, SLOT(pauseDrawing()));
#ifndef Q_OS_MAC
	connect(m_controller, SIGNAL(gamePaused(GBAThread*)), menuBar(), SLOT(show()));
	connect(m_controller, &GameController::gameUnpaused, [this]() {
		if(isFullScreen()) {
			menuBar()->hide();
		}
	});
#endif
	connect(m_controller, SIGNAL(gameUnpaused(GBAThread*)), m_display, SLOT(unpauseDrawing()));
	connect(m_controller, SIGNAL(postLog(int, const QString&)), m_logView, SLOT(postLog(int, const QString&)));
	connect(m_controller, SIGNAL(frameAvailable(const uint32_t*)), this, SLOT(recordFrame()));
	connect(m_controller, SIGNAL(gameCrashed(const QString&)), this, SLOT(gameCrashed(const QString&)));
	connect(m_controller, SIGNAL(gameFailed()), this, SLOT(gameFailed()));
	connect(m_controller, SIGNAL(unimplementedBiosCall(int)), this, SLOT(unimplementedBiosCall(int)));
	connect(m_logView, SIGNAL(levelsSet(int)), m_controller, SLOT(setLogLevel(int)));
	connect(m_logView, SIGNAL(levelsEnabled(int)), m_controller, SLOT(enableLogLevel(int)));
	connect(m_logView, SIGNAL(levelsDisabled(int)), m_controller, SLOT(disableLogLevel(int)));
	connect(this, SIGNAL(startDrawing(const uint32_t*, GBAThread*)), m_display, SLOT(startDrawing(const uint32_t*, GBAThread*)), Qt::QueuedConnection);
	connect(this, SIGNAL(shutdown()), m_display, SLOT(stopDrawing()));
	connect(this, SIGNAL(shutdown()), m_controller, SLOT(closeGame()));
	connect(this, SIGNAL(shutdown()), m_logView, SLOT(hide()));
	connect(this, SIGNAL(audioBufferSamplesChanged(int)), m_controller, SLOT(setAudioBufferSamples(int)));
	connect(this, SIGNAL(fpsTargetChanged(float)), m_controller, SLOT(setFPSTarget(float)));
	connect(&m_fpsTimer, SIGNAL(timeout()), this, SLOT(showFPS()));

	m_logView->setLevels(GBA_LOG_WARN | GBA_LOG_ERROR | GBA_LOG_FATAL);
	m_fpsTimer.setInterval(FPS_TIMER_INTERVAL);

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
}

void Window::argumentsPassed(GBAArguments* args) {
	loadConfig();

	if (args->patch) {
		m_controller->loadPatch(args->patch);
	}

	if (args->fname) {
		m_controller->loadGame(args->fname, args->dirmode);
	}
}

void Window::resizeFrame(int width, int height) {
	QSize newSize(width, height);
	newSize -= m_screenWidget->size();
	newSize += size();
	resize(newSize);
}

void Window::setConfig(ConfigController* config) {
	m_config = config;
}

void Window::loadConfig() {
	const GBAOptions* opts = m_config->options();

	m_logView->setLevels(opts->logLevel);

	m_controller->setOptions(opts);
	m_display->lockAspectRatio(opts->lockAspectRatio);
	m_display->filter(opts->resampleVideo);

	if (opts->bios) {
		m_controller->loadBIOS(opts->bios);
	}

	if (opts->fpsTarget) {
		emit fpsTargetChanged(opts->fpsTarget);
	}

	if (opts->audioBuffers) {
		emit audioBufferSamplesChanged(opts->audioBuffers);
	}

	if (opts->width && opts->height) {
		resizeFrame(opts->width, opts->height);
	}

	if (opts->fullscreen) {
		enterFullScreen();
	}

	m_mruFiles = m_config->getMRU();
	updateMRU();

	m_inputController.setConfiguration(m_config);
}

void Window::saveConfig() {
	m_config->write();
}

void Window::selectROM() {
	bool doPause = m_controller->isLoaded() && !m_controller->isPaused();
	if (doPause) {
		m_controller->setPaused(true);
	}
	QStringList formats{
		"*.gba",
#ifdef USE_LIBZIP
		"*.zip",
#endif
#ifdef USE_LZMA
		"*.7z",
#endif
		"*.rom",
		"*.bin"};
	QString filter = tr("Game Boy Advance ROMs (%1)").arg(formats.join(QChar(' ')));
	QString filename = QFileDialog::getOpenFileName(this, tr("Select ROM"), m_config->getQtOption("lastDirectory").toString(), filter);
	if (doPause) {
		m_controller->setPaused(false);
	}
	if (!filename.isEmpty()) {
		m_config->setQtOption("lastDirectory", QFileInfo(filename).dir().path());
		m_controller->loadGame(filename);
	}
}

void Window::selectBIOS() {
	bool doPause = m_controller->isLoaded() && !m_controller->isPaused();
	if (doPause) {
		m_controller->setPaused(true);
	}
	QString filename = QFileDialog::getOpenFileName(this, tr("Select BIOS"), m_config->getQtOption("lastDirectory").toString());
	if (doPause) {
		m_controller->setPaused(false);
	}
	if (!filename.isEmpty()) {
		m_config->setQtOption("lastDirectory", QFileInfo(filename).dir().path());
		m_config->setOption("bios", filename);
		m_config->updateOption("bios");
		m_config->setOption("useBios", true);
		m_config->updateOption("useBios");
		m_controller->loadBIOS(filename);
	}
}

void Window::selectPatch() {
	bool doPause = m_controller->isLoaded() && !m_controller->isPaused();
	if (doPause) {
		m_controller->setPaused(true);
	}
	QString filename = QFileDialog::getOpenFileName(this, tr("Select patch"), m_config->getQtOption("lastDirectory").toString(), tr("Patches (*.ips *.ups *.bps)"));
	if (doPause) {
		m_controller->setPaused(false);
	}
	if (!filename.isEmpty()) {
		m_config->setQtOption("lastDirectory", QFileInfo(filename).dir().path());
		m_controller->loadPatch(filename);
	}
}

void Window::openKeymapWindow() {
	GBAKeyEditor* keyEditor = new GBAKeyEditor(&m_inputController, InputController::KEYBOARD);
	connect(this, SIGNAL(shutdown()), keyEditor, SLOT(close()));
	keyEditor->setAttribute(Qt::WA_DeleteOnClose);
	keyEditor->show();
}

void Window::openSettingsWindow() {
	SettingsView* settingsWindow = new SettingsView(m_config);
	connect(this, SIGNAL(shutdown()), settingsWindow, SLOT(close()));
	connect(settingsWindow, SIGNAL(biosLoaded(const QString&)), m_controller, SLOT(loadBIOS(const QString&)));
	connect(settingsWindow, SIGNAL(audioDriverChanged()), m_controller, SLOT(reloadAudioDriver()));
	settingsWindow->setAttribute(Qt::WA_DeleteOnClose);
	settingsWindow->show();
}

void Window::openShortcutWindow() {
	ShortcutView* shortcutView = new ShortcutView();
	shortcutView->setController(m_shortcutController);
	connect(this, SIGNAL(shutdown()), shortcutView, SLOT(close()));
	shortcutView->setAttribute(Qt::WA_DeleteOnClose);
	shortcutView->show();
}

void Window::openOverrideWindow() {
	OverrideView* overrideWindow = new OverrideView(m_controller, m_config);
	connect(this, SIGNAL(shutdown()), overrideWindow, SLOT(close()));
	overrideWindow->setAttribute(Qt::WA_DeleteOnClose);
	overrideWindow->show();
}

void Window::openSensorWindow() {
	SensorView* sensorWindow = new SensorView(m_controller);
	connect(this, SIGNAL(shutdown()), sensorWindow, SLOT(close()));
	sensorWindow->setAttribute(Qt::WA_DeleteOnClose);
	sensorWindow->show();
}

void Window::openCheatsWindow() {
	CheatsView* cheatsWindow = new CheatsView(m_controller);
	connect(this, SIGNAL(shutdown()), cheatsWindow, SLOT(close()));
	cheatsWindow->setAttribute(Qt::WA_DeleteOnClose);
	cheatsWindow->show();
}

#ifdef BUILD_SDL
void Window::openGamepadWindow() {
	const char* profile = m_inputController.profileForType(SDL_BINDING_BUTTON);
	GBAKeyEditor* keyEditor = new GBAKeyEditor(&m_inputController, SDL_BINDING_BUTTON, profile);
	connect(this, SIGNAL(shutdown()), keyEditor, SLOT(close()));
	keyEditor->setAttribute(Qt::WA_DeleteOnClose);
	keyEditor->show();
}
#endif

#ifdef USE_FFMPEG
void Window::openVideoWindow() {
	if (!m_videoView) {
		m_videoView = new VideoView();
		connect(m_videoView, SIGNAL(recordingStarted(GBAAVStream*)), m_controller, SLOT(setAVStream(GBAAVStream*)));
		connect(m_videoView, SIGNAL(recordingStopped()), m_controller, SLOT(clearAVStream()), Qt::DirectConnection);
		connect(m_controller, SIGNAL(gameStopped(GBAThread*)), m_videoView, SLOT(stopRecording()));
		connect(m_controller, SIGNAL(gameStopped(GBAThread*)), m_videoView, SLOT(close()));
		connect(this, SIGNAL(shutdown()), m_videoView, SLOT(close()));
	}
	m_videoView->show();
}
#endif

#ifdef USE_MAGICK
void Window::openGIFWindow() {
	if (!m_gifView) {
		m_gifView = new GIFView();
		connect(m_gifView, SIGNAL(recordingStarted(GBAAVStream*)), m_controller, SLOT(setAVStream(GBAAVStream*)));
		connect(m_gifView, SIGNAL(recordingStopped()), m_controller, SLOT(clearAVStream()), Qt::DirectConnection);
		connect(m_controller, SIGNAL(gameStopped(GBAThread*)), m_gifView, SLOT(stopRecording()));
		connect(m_controller, SIGNAL(gameStopped(GBAThread*)), m_gifView, SLOT(close()));
		connect(this, SIGNAL(shutdown()), m_gifView, SLOT(close()));
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
	connect(this, SIGNAL(shutdown()), window, SLOT(close()));
	window->setAttribute(Qt::WA_DeleteOnClose);
	window->show();
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

void Window::resizeEvent(QResizeEvent*) {
	m_config->setOption("height", m_screenWidget->height());
	m_config->setOption("width", m_screenWidget->width());
	m_config->setOption("fullscreen", isFullScreen());
}

void Window::closeEvent(QCloseEvent* event) {
	emit shutdown();
	m_config->setQtOption("windowPos", pos());
	QMainWindow::closeEvent(event);
}

void Window::focusOutEvent(QFocusEvent*) {
	m_controller->setTurbo(false, false);
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
	m_controller->loadGame(url.path());
}

void Window::mouseDoubleClickEvent(QMouseEvent* event) {
	if (event->button() != Qt::LeftButton) {
		return;
	}
	toggleFullScreen();
}

void Window::enterFullScreen() {
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
	showNormal();
	menuBar()->show();
}

void Window::toggleFullScreen() {
	if (isFullScreen()) {
		exitFullScreen();
	} else {
		enterFullScreen();
	}
}

void Window::gameStarted(GBAThread* context) {
	char title[13] = { '\0' };
	MutexLock(&context->stateMutex);
	if (context->state < THREAD_EXITING) {
		emit startDrawing(m_controller->drawContext(), context);
		GBAGetGameTitle(context->gba, title);
	} else {
		MutexUnlock(&context->stateMutex);
		return;
	}
	MutexUnlock(&context->stateMutex);
	foreach (QAction* action, m_gameActions) {
		action->setDisabled(false);
	}
	appendMRU(context->fname);
	setWindowTitle(tr("%1 - %2").arg(projectName).arg(title));
	attachWidget(m_display);

#ifndef Q_OS_MAC
	if(isFullScreen()) {
		menuBar()->hide();
	}
#endif

	m_hitUnimplementedBiosCall = false;
	m_fpsTimer.start();
}

void Window::gameStopped() {
	foreach (QAction* action, m_gameActions) {
		action->setDisabled(true);
	}
	setWindowTitle(projectName);
	detachWidget(m_display);
	m_screenWidget->setLockAspectRatio(m_logo.width(), m_logo.height());
	m_screenWidget->setPixmap(m_logo);

	m_fpsTimer.stop();
}

void Window::gameCrashed(const QString& errorMessage) {
	QMessageBox* crash = new QMessageBox(QMessageBox::Critical, tr("Crash"),
		tr("The game has crashed with the following error:\n\n%1").arg(errorMessage),
		QMessageBox::Ok, this,  Qt::Sheet);
	crash->setAttribute(Qt::WA_DeleteOnClose);
	crash->show();
}

void Window::gameFailed() {
	QMessageBox* fail = new QMessageBox(QMessageBox::Warning, tr("Couldn't Load"),
		tr("Could not load game. Are you sure it's in the correct format?"),
		QMessageBox::Ok, this,  Qt::Sheet);
	fail->setAttribute(Qt::WA_DeleteOnClose);
	fail->show();
}

void Window::unimplementedBiosCall(int call) {
	if (m_hitUnimplementedBiosCall) {
		return;
	}
	m_hitUnimplementedBiosCall = true;

	QMessageBox* fail = new QMessageBox(QMessageBox::Warning, tr("Unimplemented BIOS call"),
		tr("This game uses a BIOS call that is not implemented. Please use the official BIOS for best experience."),
		QMessageBox::Ok, this,  Qt::Sheet);
	fail->setAttribute(Qt::WA_DeleteOnClose);
	fail->show();
}

void Window::recordFrame() {
	m_frameList.append(QDateTime::currentDateTime());
	while (m_frameList.count() > FRAME_LIST_SIZE) {
		m_frameList.removeFirst();
	}
}

void Window::showFPS() {
	char gameTitle[13] = { '\0' };
	GBAGetGameTitle(m_controller->thread()->gba, gameTitle);

	QString title(gameTitle);
	std::shared_ptr<MultiplayerController> multiplayer = m_controller->multiplayerController();
	if (multiplayer && multiplayer->attached() > 1) {
		title += tr(" -  Player %1 of %2").arg(m_playerId + 1).arg(multiplayer->attached());
	}
	if (m_frameList.isEmpty()) {
		setWindowTitle(tr("%1 - %2").arg(projectName).arg(title));
		return;
	}
	qint64 interval = m_frameList.first().msecsTo(m_frameList.last());
	float fps = (m_frameList.count() - 1) * 10000.f / interval;
	fps = round(fps) / 10.f;
	setWindowTitle(tr("%1 - %2 (%3 fps)").arg(projectName).arg(title).arg(fps));
}

void Window::openStateWindow(LoadSave ls) {
	if (m_stateWindow) {
		return;
	}
	bool wasPaused = m_controller->isPaused();
	m_stateWindow = new LoadSaveState(m_controller);
	connect(this, SIGNAL(shutdown()), m_stateWindow, SLOT(close()));
	connect(m_controller, SIGNAL(gameStopped(GBAThread*)), m_stateWindow, SLOT(close()));
	connect(m_stateWindow, &LoadSaveState::closed, [this]() {
		m_screenWidget->layout()->removeWidget(m_stateWindow);
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
	addControlledAction(fileMenu, fileMenu->addAction(tr("Load &ROM..."), this, SLOT(selectROM()), QKeySequence::Open), "loadROM");
	addControlledAction(fileMenu, fileMenu->addAction(tr("Load &BIOS..."), this, SLOT(selectBIOS())), "loadBIOS");
	addControlledAction(fileMenu, fileMenu->addAction(tr("Load &patch..."), this, SLOT(selectPatch())), "loadPatch");

	m_mruMenu = fileMenu->addMenu(tr("Recent"));

	fileMenu->addSeparator();

	QAction* loadState = new QAction(tr("&Load state"), fileMenu);
	loadState->setShortcut(tr("F10"));
	connect(loadState, &QAction::triggered, [this]() { this->openStateWindow(LoadSave::LOAD); });
	m_gameActions.append(loadState);
	addControlledAction(fileMenu, loadState, "loadState");

	QAction* saveState = new QAction(tr("&Save state"), fileMenu);
	saveState->setShortcut(tr("Shift+F10"));
	connect(saveState, &QAction::triggered, [this]() { this->openStateWindow(LoadSave::SAVE); });
	m_gameActions.append(saveState);
	addControlledAction(fileMenu, saveState, "saveState");

	QMenu* quickLoadMenu = fileMenu->addMenu(tr("Quick load"));
	QMenu* quickSaveMenu = fileMenu->addMenu(tr("Quick save"));
	int i;
	for (i = 1; i < 10; ++i) {
		QAction* quickLoad = new QAction(tr("State &%1").arg(i), quickLoadMenu);
		quickLoad->setShortcut(tr("F%1").arg(i));
		connect(quickLoad, &QAction::triggered, [this, i]() { m_controller->loadState(i); });
		m_gameActions.append(quickLoad);
		addAction(quickLoad);
		quickLoadMenu->addAction(quickLoad);

		QAction* quickSave = new QAction(tr("State &%1").arg(i), quickSaveMenu);
		quickSave->setShortcut(tr("Shift+F%1").arg(i));
		connect(quickSave, &QAction::triggered, [this, i]() { m_controller->saveState(i); });
		m_gameActions.append(quickSave);
		addAction(quickSave);
		quickSaveMenu->addAction(quickSave);
	}

	fileMenu->addSeparator();
	QAction* multiWindow = new QAction(tr("New multiplayer window"), fileMenu);
	connect(multiWindow, &QAction::triggered, [this]() {
		std::shared_ptr<MultiplayerController> multiplayer = m_controller->multiplayerController();
		if (!multiplayer) {
			multiplayer = std::make_shared<MultiplayerController>();
			m_controller->setMultiplayerController(multiplayer);
		}
		Window* w2 = new Window(m_config, multiplayer->attached());
		w2->setAttribute(Qt::WA_DeleteOnClose);
#ifndef Q_OS_MAC
		w2->show();
#endif
		w2->loadConfig();
		w2->controller()->setMultiplayerController(multiplayer);
#ifdef Q_OS_MAC
		w2->show();
#endif
	});
	addControlledAction(fileMenu, multiWindow, "multiWindow");

#ifndef Q_OS_MAC
	addControlledAction(fileMenu, fileMenu->addAction(tr("E&xit"), this, SLOT(close()), QKeySequence::Quit), "quit");
#endif

	QMenu* emulationMenu = menubar->addMenu(tr("&Emulation"));
	m_shortcutController->addMenu(emulationMenu);
	QAction* reset = new QAction(tr("&Reset"), emulationMenu);
	reset->setShortcut(tr("Ctrl+R"));
	connect(reset, SIGNAL(triggered()), m_controller, SLOT(reset()));
	m_gameActions.append(reset);
	addControlledAction(emulationMenu, reset, "reset");

	QAction* shutdown = new QAction(tr("Sh&utdown"), emulationMenu);
	connect(shutdown, SIGNAL(triggered()), m_controller, SLOT(closeGame()));
	m_gameActions.append(shutdown);
	addControlledAction(emulationMenu, shutdown, "shutdown");
	emulationMenu->addSeparator();

	QAction* pause = new QAction(tr("&Pause"), emulationMenu);
	pause->setChecked(false);
	pause->setCheckable(true);
	pause->setShortcut(tr("Ctrl+P"));
	connect(pause, SIGNAL(triggered(bool)), m_controller, SLOT(setPaused(bool)));
	connect(m_controller, &GameController::gamePaused, [this, pause]() {
		pause->setChecked(true);

		QImage currentImage(reinterpret_cast<const uchar*>(m_controller->drawContext()), VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS, 1024, QImage::Format_RGB32);
		QPixmap pixmap;
		pixmap.convertFromImage(currentImage.rgbSwapped());
		m_screenWidget->setPixmap(pixmap);
		m_screenWidget->setLockAspectRatio(3, 2);
	});
	connect(m_controller, &GameController::gameUnpaused, [pause]() { pause->setChecked(false); });
	m_gameActions.append(pause);
	addControlledAction(emulationMenu, pause, "pause");

	QAction* frameAdvance = new QAction(tr("&Next frame"), emulationMenu);
	frameAdvance->setShortcut(tr("Ctrl+N"));
	connect(frameAdvance, SIGNAL(triggered()), m_controller, SLOT(frameAdvance()));
	m_gameActions.append(frameAdvance);
	addControlledAction(emulationMenu, frameAdvance, "frameAdvance");

	emulationMenu->addSeparator();

	QAction* turbo = new QAction(tr("&Fast forward"), emulationMenu);
	turbo->setCheckable(true);
	turbo->setChecked(false);
	turbo->setShortcut(tr("Shift+Tab"));
	connect(turbo, SIGNAL(triggered(bool)), m_controller, SLOT(setTurbo(bool)));
	addControlledAction(emulationMenu, turbo, "fastForward");

	QAction* rewind = new QAction(tr("Re&wind"), emulationMenu);
	rewind->setShortcut(tr("`"));
	connect(rewind, SIGNAL(triggered()), m_controller, SLOT(rewind()));
	m_gameActions.append(rewind);
	addControlledAction(emulationMenu, rewind, "rewind");

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
	connect(solarIncrease, SIGNAL(triggered()), m_controller, SLOT(increaseLuminanceLevel()));
	addControlledAction(solarMenu, solarIncrease, "increaseLuminanceLevel");

	QAction* solarDecrease = new QAction(tr("Decrease solar level"), solarMenu);
	connect(solarDecrease, SIGNAL(triggered()), m_controller, SLOT(decreaseLuminanceLevel()));
	addControlledAction(solarMenu, solarDecrease, "decreaseLuminanceLevel");

	QAction* maxSolar = new QAction(tr("Brightest solar level"), solarMenu);
	connect(maxSolar, &QAction::triggered, [this]() { m_controller->setLuminanceLevel(10); });
	addControlledAction(solarMenu, maxSolar, "maxLuminanceLevel");

	QAction* minSolar = new QAction(tr("Darkest solar level"), solarMenu);
	connect(minSolar, &QAction::triggered, [this]() { m_controller->setLuminanceLevel(0); });
	addControlledAction(solarMenu, minSolar, "minLuminanceLevel");

	QMenu* avMenu = menubar->addMenu(tr("Audio/&Video"));
	m_shortcutController->addMenu(avMenu);
	QMenu* frameMenu = avMenu->addMenu(tr("Frame size"));
	m_shortcutController->addMenu(frameMenu, avMenu);
	for (int i = 1; i <= 6; ++i) {
		QAction* setSize = new QAction(tr("%1x").arg(QString::number(i)), avMenu);
		connect(setSize, &QAction::triggered, [this, i]() {
			showNormal();
			resizeFrame(VIDEO_HORIZONTAL_PIXELS * i, VIDEO_VERTICAL_PIXELS * i);
		});
		addControlledAction(frameMenu, setSize, QString("frame%1x").arg(QString::number(i)));
	}
	addControlledAction(frameMenu, frameMenu->addAction(tr("Fullscreen"), this, SLOT(toggleFullScreen()), QKeySequence("Ctrl+F")), "fullscreen");

	ConfigOption* lockAspectRatio = m_config->addOption("lockAspectRatio");
	lockAspectRatio->addBoolean(tr("Lock aspect ratio"), avMenu);
	lockAspectRatio->connect([this](const QVariant& value) {
		m_display->lockAspectRatio(value.toBool());
	}, this);
	m_config->updateOption("lockAspectRatio");

	ConfigOption* resampleVideo = m_config->addOption("resampleVideo");
	resampleVideo->addBoolean(tr("Resample video"), avMenu);
	resampleVideo->connect([this](const QVariant& value) {
		m_display->filter(value.toBool());
	}, this);
	m_config->updateOption("resampleVideo");

	QMenu* skipMenu = avMenu->addMenu(tr("Frame&skip"));
	ConfigOption* skip = m_config->addOption("frameskip");
	skip->connect([this](const QVariant& value) {
		m_controller->setFrameskip(value.toInt());
	}, this);
	for (int i = 0; i <= 10; ++i) {
		skip->addValue(QString::number(i), i, skipMenu);
	}
	m_config->updateOption("frameskip");

	avMenu->addSeparator();

	QMenu* buffersMenu = avMenu->addMenu(tr("Audio buffer &size"));
	ConfigOption* buffers = m_config->addOption("audioBuffers");
	buffers->connect([this](const QVariant& value) {
		emit audioBufferSamplesChanged(value.toInt());
	}, this);
	buffers->addValue(tr("512"), 512, buffersMenu);
	buffers->addValue(tr("768"), 768, buffersMenu);
	buffers->addValue(tr("1024"), 1024, buffersMenu);
	buffers->addValue(tr("2048"), 2048, buffersMenu);
	buffers->addValue(tr("4096"), 4096, buffersMenu);
	m_config->updateOption("audioBuffers");

	avMenu->addSeparator();

	QMenu* target = avMenu->addMenu(tr("FPS target"));
	ConfigOption* fpsTargetOption = m_config->addOption("fpsTarget");
	fpsTargetOption->connect([this](const QVariant& value) {
		emit fpsTargetChanged(value.toInt());
	}, this);
	fpsTargetOption->addValue(tr("15"), 15, target);
	fpsTargetOption->addValue(tr("30"), 30, target);
	fpsTargetOption->addValue(tr("45"), 45, target);
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
	connect(screenshot, SIGNAL(triggered()), m_display, SLOT(screenshot()));
	m_gameActions.append(screenshot);
	addControlledAction(avMenu, screenshot, "screenshot");
#endif

#ifdef USE_FFMPEG
	QAction* recordOutput = new QAction(tr("Record output..."), avMenu);
	recordOutput->setShortcut(tr("F11"));
	connect(recordOutput, SIGNAL(triggered()), this, SLOT(openVideoWindow()));
	addControlledAction(avMenu, recordOutput, "recordOutput");
#endif

#ifdef USE_MAGICK
	QAction* recordGIF = new QAction(tr("Record GIF..."), avMenu);
	recordGIF->setShortcut(tr("Shift+F11"));
	connect(recordGIF, SIGNAL(triggered()), this, SLOT(openGIFWindow()));
	addControlledAction(avMenu, recordGIF, "recordGIF");
#endif

	QMenu* toolsMenu = menubar->addMenu(tr("&Tools"));
	m_shortcutController->addMenu(toolsMenu);
	QAction* viewLogs = new QAction(tr("View &logs..."), toolsMenu);
	connect(viewLogs, SIGNAL(triggered()), m_logView, SLOT(show()));
	addControlledAction(toolsMenu, viewLogs, "viewLogs");

	QAction* overrides = new QAction(tr("Game &overrides..."), toolsMenu);
	connect(overrides, SIGNAL(triggered()), this, SLOT(openOverrideWindow()));
	addControlledAction(toolsMenu, overrides, "overrideWindow");

	QAction* sensors = new QAction(tr("Game &Pak sensors..."), toolsMenu);
	connect(sensors, SIGNAL(triggered()), this, SLOT(openSensorWindow()));
	addControlledAction(toolsMenu, sensors, "sensorWindow");

	QAction* cheats = new QAction(tr("&Cheats..."), toolsMenu);
	connect(cheats, SIGNAL(triggered()), this, SLOT(openCheatsWindow()));
	addControlledAction(toolsMenu, cheats, "cheatsWindow");

#ifdef USE_GDB_STUB
	QAction* gdbWindow = new QAction(tr("Start &GDB server..."), toolsMenu);
	connect(gdbWindow, SIGNAL(triggered()), this, SLOT(gdbOpen()));
	addControlledAction(toolsMenu, gdbWindow, "gdbWindow");
#endif

	toolsMenu->addSeparator();
	addControlledAction(toolsMenu, toolsMenu->addAction(tr("Settings..."), this, SLOT(openSettingsWindow())), "settings");
	addControlledAction(toolsMenu, toolsMenu->addAction(tr("Edit shortcuts..."), this, SLOT(openShortcutWindow())), "shortcuts");

	QAction* keymap = new QAction(tr("Remap keyboard..."), toolsMenu);
	connect(keymap, SIGNAL(triggered()), this, SLOT(openKeymapWindow()));
	addControlledAction(toolsMenu, keymap, "remapKeyboard");

#ifdef BUILD_SDL
	QAction* gamepad = new QAction(tr("Remap gamepad..."), toolsMenu);
	connect(gamepad, SIGNAL(triggered()), this, SLOT(openGamepadWindow()));
	addControlledAction(toolsMenu, gamepad, "remapGamepad");
#endif

	ConfigOption* skipBios = m_config->addOption("skipBios");
	skipBios->connect([this](const QVariant& value) {
		m_controller->setSkipBIOS(value.toBool());
	}, this);

	ConfigOption* useBios = m_config->addOption("useBios");
	useBios->connect([this](const QVariant& value) {
		m_controller->setUseBIOS(value.toBool());
	}, this);

	ConfigOption* rewindEnable = m_config->addOption("rewindEnable");
	rewindEnable->connect([this](const QVariant& value) {
		m_controller->setRewind(value.toBool(), m_config->getOption("rewindBufferCapacity").toInt(), m_config->getOption("rewindBufferInterval").toInt());
	}, this);

	ConfigOption* rewindBufferCapacity = m_config->addOption("rewindBufferCapacity");
	rewindBufferCapacity->connect([this](const QVariant& value) {
		m_controller->setRewind(m_config->getOption("rewindEnable").toInt(), value.toInt(), m_config->getOption("rewindBufferInterval").toInt());
	}, this);

	ConfigOption* rewindBufferInterval = m_config->addOption("rewindBufferInterval");
	rewindBufferInterval->connect([this](const QVariant& value) {
		m_controller->setRewind(m_config->getOption("rewindEnable").toInt(), m_config->getOption("rewindBufferCapacity").toInt(), value.toInt());
	}, this);

	ConfigOption* allowOpposingDirections = m_config->addOption("allowOpposingDirections");
	allowOpposingDirections->connect([this](const QVariant& value) {
		m_inputController.setAllowOpposing(value.toBool());
	}, this);

	QMenu* other = new QMenu(tr("Other"), this);
	m_shortcutController->addMenu(other);
	m_shortcutController->addFunctions(other, [this]() {
		m_controller->setTurbo(true, false);
	}, [this]() {
		m_controller->setTurbo(false, false);
	}, QKeySequence(Qt::Key_Tab), tr("Fast Forward (held)"), "holdFastForward");

	addControlledAction(other, other->addAction(tr("Exit fullscreen"), this, SLOT(exitFullScreen()), QKeySequence("Esc")), "exitFullScreen");

	foreach (QAction* action, m_gameActions) {
		action->setDisabled(true);
	}
}

void Window::attachWidget(QWidget* widget) {
	m_screenWidget->layout()->addWidget(widget);
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
	m_mruMenu->clear();
	int i = 0;
	for (const QString& file : m_mruFiles) {
		QAction* item = new QAction(file, m_mruMenu);
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
	m_shortcutController->addAction(menu, action, name);
	menu->addAction(action);
	action->setShortcutContext(Qt::WidgetShortcut);
	addAction(action);
	return action;
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

void WindowBackground::setLockAspectRatio(int width, int height) {
	m_aspectWidth = width;
	m_aspectHeight = height;
}

void WindowBackground::paintEvent(QPaintEvent*) {
	QPainter painter(this);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);
	const QPixmap* logo = pixmap();
	painter.fillRect(QRect(QPoint(), size()), Qt::black);
	if (!logo) {
		return;
	}
	QSize s = size();
	QSize ds = s;
	if (s.width() * m_aspectHeight > s.height() * m_aspectWidth) {
		ds.setWidth(s.height() * m_aspectWidth / m_aspectHeight);
	} else if (s.width() * m_aspectHeight < s.height() * m_aspectWidth) {
		ds.setHeight(s.width() * m_aspectHeight / m_aspectWidth);
	}
	QPoint origin = QPoint((s.width() - ds.width()) / 2, (s.height() - ds.height()) / 2);
	QRect full(origin, ds);
	painter.drawPixmap(full, *logo);
}
