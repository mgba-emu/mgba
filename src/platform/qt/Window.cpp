#include "Window.h"

#include <QFileDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenuBar>

#include "GameController.h"
#include "GDBWindow.h"
#include "GDBController.h"

using namespace QGBA;

Window::Window(QWidget* parent)
	: QMainWindow(parent)
#ifdef USE_GDB_STUB
	, m_gdbController(nullptr)
#endif
{
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	setMinimumSize(240, 160);

	m_controller = new GameController(this);
	m_display = new Display();
	setCentralWidget(m_display);
	connect(m_controller, SIGNAL(gameStarted(GBAThread*)), this, SLOT(gameStarted(GBAThread*)));
	connect(m_controller, SIGNAL(gameStopped(GBAThread*)), m_display, SLOT(stopDrawing()));
	connect(m_controller, SIGNAL(gameStopped(GBAThread*)), this, SLOT(gameStopped()));
	connect(this, SIGNAL(startDrawing(const uint32_t*, GBAThread*)), m_display, SLOT(startDrawing(const uint32_t*, GBAThread*)), Qt::QueuedConnection);
	connect(this, SIGNAL(shutdown()), m_display, SLOT(stopDrawing()));
	connect(this, SIGNAL(audioBufferSamplesChanged(int)), m_controller, SLOT(setAudioBufferSamples(int)));

	setupMenu(menuBar());
}

GBAKey Window::mapKey(int qtKey) {
	switch (qtKey) {
	case Qt::Key_Z:
		return GBA_KEY_A;
		break;
	case Qt::Key_X:
		return GBA_KEY_B;
		break;
	case Qt::Key_A:
		return GBA_KEY_L;
		break;
	case Qt::Key_S:
		return GBA_KEY_R;
		break;
	case Qt::Key_Return:
		return GBA_KEY_START;
		break;
	case Qt::Key_Backspace:
		return GBA_KEY_SELECT;
		break;
	case Qt::Key_Up:
		return GBA_KEY_UP;
		break;
	case Qt::Key_Down:
		return GBA_KEY_DOWN;
		break;
	case Qt::Key_Left:
		return GBA_KEY_LEFT;
		break;
	case Qt::Key_Right:
		return GBA_KEY_RIGHT;
		break;
	default:
		return GBA_KEY_NONE;
	}
}

void Window::selectROM() {
	QString filename = QFileDialog::getOpenFileName(this, tr("Select ROM"));
	if (!filename.isEmpty()) {
		m_controller->loadGame(filename);
	}
}

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
	GBAKey key = mapKey(event->key());
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
	GBAKey key = mapKey(event->key());
	if (key == GBA_KEY_NONE) {
		QWidget::keyPressEvent(event);
		return;
	}
	m_controller->keyReleased(key);
	event->accept();
}

void Window::closeEvent(QCloseEvent* event) {
	emit shutdown();
	QMainWindow::closeEvent(event);
}

void Window::gameStarted(GBAThread* context) {
	emit startDrawing(m_controller->drawContext(), context);
	foreach (QAction* action, m_gameActions) {
		action->setDisabled(false);
	}
}

void Window::gameStopped() {
	foreach (QAction* action, m_gameActions) {
		action->setDisabled(true);
	}
}

void Window::setBuffers512() {
	emit audioBufferSamplesChanged(512);
}

void Window::setBuffers1024() {
	emit audioBufferSamplesChanged(1024);
}

void Window::setBuffers2048() {
	emit audioBufferSamplesChanged(2048);
}

void Window::setBuffers4096() {
	emit audioBufferSamplesChanged(4096);
}

void Window::setupMenu(QMenuBar* menubar) {
	menubar->clear();
	QMenu* fileMenu = menubar->addMenu(tr("&File"));
	fileMenu->addAction(tr("Load &ROM..."), this, SLOT(selectROM()), QKeySequence::Open);
	fileMenu->addAction(tr("Sh&utdown"), m_controller, SLOT(closeGame()));

	QMenu* emulationMenu = menubar->addMenu(tr("&Emulation"));
	QAction* pause = new QAction(tr("&Pause"), nullptr);
	pause->setChecked(false);
	pause->setCheckable(true);
	pause->setShortcut(tr("Ctrl+P"));
	pause->setDisabled(true);
	connect(pause, SIGNAL(triggered(bool)), m_controller, SLOT(setPaused(bool)));
	m_gameActions.append(pause);
	emulationMenu->addAction(pause);

	QAction* frameAdvance = new QAction(tr("&Next frame"), nullptr);
	frameAdvance->setShortcut(tr("Ctrl+N"));
	frameAdvance->setDisabled(true);
	connect(frameAdvance, SIGNAL(triggered()), m_controller, SLOT(frameAdvance()));
	m_gameActions.append(frameAdvance);
	emulationMenu->addAction(frameAdvance);

	QMenu* soundMenu = menubar->addMenu(tr("&Sound"));
	QMenu* buffersMenu = soundMenu->addMenu(tr("Buffer &size"));
	buffersMenu->addAction(tr("512"), this, SLOT(setBuffers512()));
	buffersMenu->addAction(tr("1024"), this, SLOT(setBuffers1024()));
	buffersMenu->addAction(tr("2048"), this, SLOT(setBuffers2048()));
	buffersMenu->addAction(tr("4096"), this, SLOT(setBuffers4096()));

	QMenu* debuggingMenu = menubar->addMenu(tr("&Debugging"));
#ifdef USE_GDB_STUB
	QAction* gdbWindow = new QAction(tr("Start &GDB server..."), nullptr);
	connect(gdbWindow, SIGNAL(triggered()), this, SLOT(gdbOpen()));
	debuggingMenu->addAction(gdbWindow);
#endif
}
