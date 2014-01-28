#include "Window.h"

#include <QFileDialog>

using namespace QGBA;

Window::Window(QWidget* parent) : QMainWindow(parent) {
	setupUi(this);

	m_controller = new GameController(this);
	m_display = new Display(this);
	setCentralWidget(m_display);
	connect(m_controller, SIGNAL(frameAvailable(const QImage&)), m_display, SLOT(draw(const QImage&)));

	connect(actionOpen, SIGNAL(triggered()), this, SLOT(selectROM()));
}

void Window::selectROM() {
	QString filename = QFileDialog::getOpenFileName(this, tr("Select ROM"));
	if (!filename.isEmpty()) {
		m_controller->loadGame(filename);
	}
}
