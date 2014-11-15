#include "Window.h"

#include <QFileDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenuBar>
#include <QStackedLayout>

#include "ConfigController.h"
#include "GameController.h"
#include "GBAKeyEditor.h"
#include "GDBController.h"
#include "GDBWindow.h"
#include "LoadSaveState.h"
#include "LogView.h"
#include "VideoView.h"

extern "C" {
#include "platform/commandline.h"
}

using namespace QGBA;

Window::Window(ConfigController* config, QWidget* parent)
	: QMainWindow(parent)
	, m_logView(new LogView())
	, m_stateWindow(nullptr)
	, m_screenWidget(new WindowBackground())
	, m_logo(":/res/mgba-1024.png")
	, m_config(config)
#ifdef USE_FFMPEG
	, m_videoView(nullptr)
#endif
#ifdef USE_GDB_STUB
	, m_gdbController(nullptr)
#endif
{
	setWindowTitle(PROJECT_NAME);
	m_controller = new GameController(this);
	m_controller->setInputController(&m_inputController);

	QGLFormat format(QGLFormat(QGL::Rgba | QGL::DoubleBuffer));
	format.setSwapInterval(1);
	m_display = new Display(format);

	m_screenWidget->setMinimumSize(m_display->minimumSize());
	m_screenWidget->setSizePolicy(m_display->sizePolicy());
	m_screenWidget->setSizeHint(m_display->minimumSize() * 2);
	setCentralWidget(m_screenWidget);

	connect(m_controller, SIGNAL(gameStarted(GBAThread*)), this, SLOT(gameStarted(GBAThread*)));
	connect(m_controller, SIGNAL(gameStopped(GBAThread*)), m_display, SLOT(stopDrawing()));
	connect(m_controller, SIGNAL(gameStopped(GBAThread*)), this, SLOT(gameStopped()));
	connect(m_controller, SIGNAL(stateLoaded(GBAThread*)), m_display, SLOT(forceDraw()));
	connect(m_controller, SIGNAL(gamePaused(GBAThread*)), m_display, SLOT(pauseDrawing()));
	connect(m_controller, SIGNAL(gameUnpaused(GBAThread*)), m_display, SLOT(unpauseDrawing()));
	connect(m_controller, SIGNAL(postLog(int, const QString&)), m_logView, SLOT(postLog(int, const QString&)));
	connect(this, SIGNAL(startDrawing(const uint32_t*, GBAThread*)), m_display, SLOT(startDrawing(const uint32_t*, GBAThread*)), Qt::QueuedConnection);
	connect(this, SIGNAL(shutdown()), m_display, SLOT(stopDrawing()));
	connect(this, SIGNAL(shutdown()), m_controller, SLOT(closeGame()));
	connect(this, SIGNAL(shutdown()), m_logView, SLOT(hide()));
	connect(this, SIGNAL(audioBufferSamplesChanged(int)), m_controller, SLOT(setAudioBufferSamples(int)));
	connect(this, SIGNAL(fpsTargetChanged(float)), m_controller, SLOT(setFPSTarget(float)));

	setupMenu(menuBar());
}

Window::~Window() {
	delete m_logView;

#ifdef USE_FFMPEG
	delete m_videoView;
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

void Window::setConfig(ConfigController* config) {
	m_config = config;
}

void Window::loadConfig() {
	const GBAOptions* opts = m_config->options();

	m_logView->setLevels(opts->logLevel);

	m_controller->setFrameskip(opts->frameskip);
	m_controller->setAudioSync(opts->audioSync);
	m_controller->setVideoSync(opts->videoSync);

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
		m_screenWidget->setSizeHint(QSize(opts->width, opts->height));
	}

	m_inputController.setConfiguration(m_config);
}

void Window::saveConfig() {
	m_config->write();
}

void Window::selectROM() {
	QString filename = QFileDialog::getOpenFileName(this, tr("Select ROM"));
	if (!filename.isEmpty()) {
		m_controller->loadGame(filename);
	}
}

void Window::selectBIOS() {
	QString filename = QFileDialog::getOpenFileName(this, tr("Select BIOS"));
	if (!filename.isEmpty()) {
		m_controller->loadBIOS(filename);
	}
}

void Window::selectPatch() {
	QString filename = QFileDialog::getOpenFileName(this, tr("Select patch"), QString(), tr("Patches (*.ips *.ups)"));
	if (!filename.isEmpty()) {
		m_controller->loadPatch(filename);
	}
}

void Window::openKeymapWindow() {
	GBAKeyEditor* keyEditor = new GBAKeyEditor(&m_inputController, InputController::KEYBOARD);
	connect(this, SIGNAL(shutdown()), keyEditor, SLOT(close()));
	keyEditor->setAttribute(Qt::WA_DeleteOnClose);
	keyEditor->show();
}

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

#ifdef USE_GDB_STUB
void Window::gdbOpen() {
	if (!m_gdbController) {
		m_gdbController = new GDBController(m_controller, this);
	}
	GDBWindow* window = new GDBWindow(m_gdbController);
	window->show();
}
#endif

void Window::keyPressEvent(QKeyEvent* event) {
	if (event->isAutoRepeat()) {
		QWidget::keyPressEvent(event);
		return;
	}
	if (event->key() == Qt::Key_Tab) {
		m_controller->setTurbo(true, false);
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
	if (event->key() == Qt::Key_Tab) {
		m_controller->setTurbo(false, false);
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
	redoLogo();
	m_config->setOption("height", m_screenWidget->height());
	m_config->setOption("width", m_screenWidget->width());
}

void Window::closeEvent(QCloseEvent* event) {
	emit shutdown();
	QMainWindow::closeEvent(event);
}

void Window::toggleFullScreen() {
	if (isFullScreen()) {
		showNormal();
	} else {
		showFullScreen();
	}
}

void Window::gameStarted(GBAThread* context) {
	emit startDrawing(m_controller->drawContext(), context);
	foreach (QAction* action, m_gameActions) {
		action->setDisabled(false);
	}
	char title[13] = { '\0' };
	GBAGetGameTitle(context->gba, title);
	setWindowTitle(tr(PROJECT_NAME " - %1").arg(title));
	attachWidget(m_display);
	m_screenWidget->setScaledContents(true);
}

void Window::gameStopped() {
	foreach (QAction* action, m_gameActions) {
		action->setDisabled(true);
	}
	setWindowTitle(tr(PROJECT_NAME));
	detachWidget(m_display);
	m_screenWidget->setScaledContents(false);
	redoLogo();
}

void Window::redoLogo() {
	if (m_controller->isLoaded()) {
		return;
	}
	QPixmap logo(m_logo.scaled(m_screenWidget->size() * m_screenWidget->devicePixelRatio(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
	logo.setDevicePixelRatio(m_screenWidget->devicePixelRatio());
	m_screenWidget->setPixmap(logo);
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
		setFocus();
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
	fileMenu->addAction(tr("Load &ROM..."), this, SLOT(selectROM()), QKeySequence::Open);
	fileMenu->addAction(tr("Load &BIOS..."), this, SLOT(selectBIOS()));
	fileMenu->addAction(tr("Load &patch..."), this, SLOT(selectPatch()));

	fileMenu->addSeparator();

	QAction* loadState = new QAction(tr("&Load state"), fileMenu);
	loadState->setShortcut(tr("F10"));
	connect(loadState, &QAction::triggered, [this]() { this->openStateWindow(LoadSave::LOAD); });
	m_gameActions.append(loadState);
	fileMenu->addAction(loadState);

	QAction* saveState = new QAction(tr("&Save state"), fileMenu);
	saveState->setShortcut(tr("Shift+F10"));
	connect(saveState, &QAction::triggered, [this]() { this->openStateWindow(LoadSave::SAVE); });
	m_gameActions.append(saveState);
	fileMenu->addAction(saveState);

	QMenu* quickLoadMenu = fileMenu->addMenu(tr("Quick load"));
	QMenu* quickSaveMenu = fileMenu->addMenu(tr("Quick save"));
	int i;
	for (i = 1; i < 10; ++i) {
		QAction* quickLoad = new QAction(tr("State &%1").arg(i), quickLoadMenu);
		quickLoad->setShortcut(tr("F%1").arg(i));
		connect(quickLoad, &QAction::triggered, [this, i]() { m_controller->loadState(i); });
		m_gameActions.append(quickLoad);
		quickLoadMenu->addAction(quickLoad);

		QAction* quickSave = new QAction(tr("State &%1").arg(i), quickSaveMenu);
		quickSave->setShortcut(tr("Shift+F%1").arg(i));
		connect(quickSave, &QAction::triggered, [this, i]() { m_controller->saveState(i); });
		m_gameActions.append(quickSave);
		quickSaveMenu->addAction(quickSave);
	}

#ifndef Q_OS_MAC
	fileMenu->addSeparator();
	fileMenu->addAction(tr("E&xit"), this, SLOT(close()), QKeySequence::Quit);
#endif

	QMenu* emulationMenu = menubar->addMenu(tr("&Emulation"));
	QAction* reset = new QAction(tr("&Reset"), emulationMenu);
	reset->setShortcut(tr("Ctrl+R"));
	connect(reset, SIGNAL(triggered()), m_controller, SLOT(reset()));
	m_gameActions.append(reset);
	emulationMenu->addAction(reset);

	QAction* shutdown = new QAction(tr("Sh&utdown"), emulationMenu);
	connect(shutdown, SIGNAL(triggered()), m_controller, SLOT(closeGame()));
	m_gameActions.append(shutdown);
	emulationMenu->addAction(shutdown);
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
	});
	connect(m_controller, &GameController::gameUnpaused, [pause]() { pause->setChecked(false); });
	m_gameActions.append(pause);
	emulationMenu->addAction(pause);

	QAction* frameAdvance = new QAction(tr("&Next frame"), emulationMenu);
	frameAdvance->setShortcut(tr("Ctrl+N"));
	connect(frameAdvance, SIGNAL(triggered()), m_controller, SLOT(frameAdvance()));
	m_gameActions.append(frameAdvance);
	emulationMenu->addAction(frameAdvance);

	emulationMenu->addSeparator();

	QAction* turbo = new QAction(tr("T&urbo"), emulationMenu);
	turbo->setCheckable(true);
	turbo->setChecked(false);
	turbo->setShortcut(tr("Shift+Tab"));
	connect(turbo, SIGNAL(triggered(bool)), m_controller, SLOT(setTurbo(bool)));
	emulationMenu->addAction(turbo);

	ConfigOption* videoSync = m_config->addOption("videoSync");
	videoSync->addBoolean(tr("Sync to &video"), emulationMenu);
	videoSync->connect([this](const QVariant& value) { m_controller->setVideoSync(value.toBool()); });
	m_config->updateOption("videoSync");

	ConfigOption* audioSync = m_config->addOption("audioSync");
	audioSync->addBoolean(tr("Sync to &audio"), emulationMenu);
	audioSync->connect([this](const QVariant& value) { m_controller->setAudioSync(value.toBool()); });
	m_config->updateOption("audioSync");

	emulationMenu->addSeparator();
	QAction* keymap = new QAction(tr("Remap keyboard..."), emulationMenu);
	connect(keymap, SIGNAL(triggered()), this, SLOT(openKeymapWindow()));
	emulationMenu->addAction(keymap);

	QMenu* avMenu = menubar->addMenu(tr("Audio/&Video"));
	QMenu* frameMenu = avMenu->addMenu(tr("Frame size"));
	QAction* setSize = new QAction(tr("1x"), avMenu);
	connect(setSize, &QAction::triggered, [this]() {
		showNormal();
		resize(VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
	});
	frameMenu->addAction(setSize);
	setSize = new QAction(tr("2x"), avMenu);
	connect(setSize, &QAction::triggered, [this]() {
		showNormal();
		resize(VIDEO_HORIZONTAL_PIXELS * 2, VIDEO_VERTICAL_PIXELS * 2);
	});
	frameMenu->addAction(setSize);
	setSize = new QAction(tr("3x"), avMenu);
	connect(setSize, &QAction::triggered, [this]() {
		showNormal();
		resize(VIDEO_HORIZONTAL_PIXELS * 3, VIDEO_VERTICAL_PIXELS * 3);
	});
	frameMenu->addAction(setSize);
	setSize = new QAction(tr("4x"), avMenu);
	connect(setSize, &QAction::triggered, [this]() {
		showNormal();
		resize(VIDEO_HORIZONTAL_PIXELS * 4, VIDEO_VERTICAL_PIXELS * 4);
	});
	frameMenu->addAction(setSize);
	frameMenu->addAction(tr("Fullscreen"), this, SLOT(toggleFullScreen()), QKeySequence("Ctrl+F"));

	QMenu* skipMenu = avMenu->addMenu(tr("Frame&skip"));
	ConfigOption* skip = m_config->addOption("frameskip");
	skip->connect([this](const QVariant& value) { m_controller->setFrameskip(value.toInt()); });
	for (int i = 0; i <= 10; ++i) {
		skip->addValue(QString::number(i), i, skipMenu);
	}
	m_config->updateOption("frameskip");

	avMenu->addSeparator();

	QMenu* buffersMenu = avMenu->addMenu(tr("Buffer &size"));
	ConfigOption* buffers = m_config->addOption("audioBuffers");
	buffers->connect([this](const QVariant& value) { emit audioBufferSamplesChanged(value.toInt()); });
	buffers->addValue(tr("512"), 512, buffersMenu);
	buffers->addValue(tr("768"), 768, buffersMenu);
	buffers->addValue(tr("1024"), 1024, buffersMenu);
	buffers->addValue(tr("2048"), 2048, buffersMenu);
	buffers->addValue(tr("4096"), 4096, buffersMenu);
	m_config->updateOption("audioBuffers");

	avMenu->addSeparator();

	QMenu* target = avMenu->addMenu("FPS target");
	ConfigOption* fpsTargetOption = m_config->addOption("fpsTarget");
	fpsTargetOption->connect([this](const QVariant& value) { emit fpsTargetChanged(value.toInt()); });
	fpsTargetOption->addValue(tr("15"), 15, target);
	fpsTargetOption->addValue(tr("30"), 30, target);
	fpsTargetOption->addValue(tr("45"), 45, target);
	fpsTargetOption->addValue(tr("60"), 60, target);
	fpsTargetOption->addValue(tr("90"), 90, target);
	fpsTargetOption->addValue(tr("120"), 120, target);
	fpsTargetOption->addValue(tr("240"), 240, target);
	m_config->updateOption("fpsTarget");

#if defined(USE_PNG) || defined(USE_FFMPEG)
	avMenu->addSeparator();
#endif

#ifdef USE_PNG
	QAction* screenshot = new QAction(tr("Take &screenshot"), avMenu);
	screenshot->setShortcut(tr("F12"));
	connect(screenshot, SIGNAL(triggered()), m_display, SLOT(screenshot()));
	m_gameActions.append(screenshot);
	avMenu->addAction(screenshot);
#endif

#ifdef USE_FFMPEG
	QAction* recordOutput = new QAction(tr("Record output..."), avMenu);
	recordOutput->setShortcut(tr("F11"));
	connect(recordOutput, SIGNAL(triggered()), this, SLOT(openVideoWindow()));
	avMenu->addAction(recordOutput);
#endif

	QMenu* debuggingMenu = menubar->addMenu(tr("&Debugging"));
	QAction* viewLogs = new QAction(tr("View &logs..."), debuggingMenu);
	connect(viewLogs, SIGNAL(triggered()), m_logView, SLOT(show()));
	debuggingMenu->addAction(viewLogs);
#ifdef USE_GDB_STUB
	QAction* gdbWindow = new QAction(tr("Start &GDB server..."), debuggingMenu);
	connect(gdbWindow, SIGNAL(triggered()), this, SLOT(gdbOpen()));
	debuggingMenu->addAction(gdbWindow);
#endif

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

WindowBackground::WindowBackground(QWidget* parent)
	: QLabel(parent)
{
	setLayout(new QStackedLayout());
	layout()->setContentsMargins(0, 0, 0, 0);
	setAlignment(Qt::AlignCenter);
	QPalette p = palette();
	p.setColor(backgroundRole(), Qt::black);
	setPalette(p);
	setAutoFillBackground(true);
}

void WindowBackground::setSizeHint(const QSize& hint) {
	m_sizeHint = hint;
}

QSize WindowBackground::sizeHint() const {
	return m_sizeHint;
}
