#include "GameController.h"

extern "C" {
#include "gba.h"
#include "renderers/video-software.h"
}

using namespace QGBA;

GameController::GameController(QObject* parent)
	: QObject(parent)
	, m_drawContext(256, 256, QImage::Format_RGB32)
	, m_audioContext(0)
{
	m_renderer = new GBAVideoSoftwareRenderer;
	GBAVideoSoftwareRendererCreate(m_renderer);
	m_renderer->outputBuffer = (color_t*) m_drawContext.bits();
	m_renderer->outputBufferStride = m_drawContext.bytesPerLine() / 4;
	m_threadContext = {
		.useDebugger = 0,
		.frameskip = 0,
		.biosFd = -1,
		.renderer = &m_renderer->d,
		.sync.videoFrameWait = 0,
		.sync.audioWait = 1,
		.userData = this,
		.rewindBufferCapacity = 0
	};
	m_threadContext.startCallback = [] (GBAThread* context) {
		GameController* controller = static_cast<GameController*>(context->userData);
		controller->audioDeviceAvailable(&context->gba->audio);
	};
	m_threadContext.frameCallback = [] (GBAThread* context) {
		GameController* controller = static_cast<GameController*>(context->userData);
		controller->frameAvailable(controller->m_drawContext);
	};
}

GameController::~GameController() {
	GBAThreadEnd(&m_threadContext);
	GBAThreadJoin(&m_threadContext);
	delete m_renderer;
}

bool GameController::loadGame(const QString& path) {
	m_rom = new QFile(path);
	if (!m_rom->open(QIODevice::ReadOnly)) {
		delete m_rom;
		m_rom = 0;
		return false;
	}
	m_threadContext.fd = m_rom->handle();
	m_threadContext.fname = path.toLocal8Bit().constData();
	GBAThreadStart(&m_threadContext);
	return true;
}

void GameController::keyPressed(int key) {
	int mappedKey = 1 << key;
	m_threadContext.activeKeys |= mappedKey;
}

void GameController::keyReleased(int key) {
	int mappedKey = 1 << key;
	m_threadContext.activeKeys &= ~mappedKey;
}
