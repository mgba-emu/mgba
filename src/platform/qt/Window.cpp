#include "Window.h"

#include <QFileDialog>
#include <QKeyEvent>

extern "C" {
#include "gba.h"
}

using namespace QGBA;

Window::Window(QWidget* parent) : QMainWindow(parent) {
	setupUi(this);

	m_controller = new GameController(this);
	m_display = new Display(this);
	setCentralWidget(m_display);
	connect(m_controller, SIGNAL(frameAvailable(const QImage&)), m_display, SLOT(draw(const QImage&)));
	connect(m_controller, SIGNAL(audioDeviceAvailable(GBAAudio*)), this, SLOT(setupAudio(GBAAudio*)));

	connect(actionOpen, SIGNAL(triggered()), this, SLOT(selectROM()));
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

void Window::setupAudio(GBAAudio* audio) {
	AudioDevice::Thread* thread = new AudioDevice::Thread(this);
	thread->setInput(audio);
	thread->start(QThread::HighPriority);
}
