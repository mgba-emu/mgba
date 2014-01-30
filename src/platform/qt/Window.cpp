#include "Window.h"

#include <QFileDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenuBar>

using namespace QGBA;

Window::Window(QWidget* parent) : QMainWindow(parent) {
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	setMinimumSize(240, 160);

	m_controller = new GameController(this);
	m_display = new Display();
	setCentralWidget(m_display);
	connect(m_controller, SIGNAL(audioDeviceAvailable(GBAAudio*)), this, SLOT(setupAudio(GBAAudio*)));
	connect(m_controller, SIGNAL(gameStarted(GBAThread*)), this, SLOT(gameStarted(GBAThread*)));
	connect(this, SIGNAL(startDrawing(const uint32_t*, GBAThread*)), m_display, SLOT(startDrawing(const uint32_t*, GBAThread*)), Qt::QueuedConnection);
	connect(this, SIGNAL(shutdown()), m_display, SLOT(stopDrawing()));

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

void Window::setupAudio(GBAAudio* audio) {
	AudioDevice::Thread* thread = new AudioDevice::Thread(this);
	thread->setInput(audio);
	thread->start(QThread::HighPriority);
}

void Window::setupMenu(QMenuBar* menubar) {
	menubar->clear();
	QMenu* fileMenu = menubar->addMenu(tr("&File"));
	fileMenu->addAction(tr("Load &ROM"), this, SLOT(selectROM()), QKeySequence::Open);

	QMenu* emulationMenu = menubar->addMenu(tr("&Emulation"));
	QAction* pause = new QAction(tr("&Pause"), 0);
	pause->setChecked(false);
	pause->setCheckable(true);
	pause->setShortcut(tr("Ctrl+P"));
	pause->setDisabled(true);
	connect(pause, SIGNAL(triggered(bool)), m_controller, SLOT(setPaused(bool)));
	m_gameActions.append(pause);
	emulationMenu->addAction(pause);

	QAction* frameAdvance = new QAction(tr("&Next frame"), 0);
	frameAdvance->setShortcut(tr("Ctrl+N"));
	frameAdvance->setDisabled(true);
	connect(frameAdvance, SIGNAL(triggered()), m_controller, SLOT(frameAdvance()));
	m_gameActions.append(frameAdvance);
	emulationMenu->addAction(frameAdvance);
}
