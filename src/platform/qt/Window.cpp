#include "Window.h"

#include <QFileDialog>

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

void Window::selectROM() {
	QString filename = QFileDialog::getOpenFileName(this, tr("Select ROM"));
	if (!filename.isEmpty()) {
		m_controller->loadGame(filename);
	}
}

void Window::setupAudio(GBAAudio* audio) {
	AudioDevice* device = new AudioDevice(audio, this);
	AudioDevice::Thread* thread = new AudioDevice::Thread(device, this);
	if (!m_audio) {
		QAudioFormat format;
		format.setSampleRate(44100);
		format.setChannelCount(2);
		format.setSampleSize(16);
		format.setCodec("audio/pcm");
		format.setByteOrder(QAudioFormat::LittleEndian);
		format.setSampleType(QAudioFormat::SignedInt);

		m_audio = new QAudioOutput(format, this);
		m_audio->setBufferSize(1024);
	}
	device->setFormat(m_audio->format());
	thread->setOutput(m_audio);
	thread->start();
}
