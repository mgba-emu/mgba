#include "GameController.h"

extern "C" {
#include "gba.h"
#include "renderers/video-software.h"
}

using namespace QGBA;

GameController::GameController(QObject* parent)
	: QObject(parent)
	, m_drawContext(new uint32_t[256 * 256])
	, m_audioContext(0)
{
	m_renderer = new GBAVideoSoftwareRenderer;
	GBAVideoSoftwareRendererCreate(m_renderer);
	m_renderer->outputBuffer = (color_t*) m_drawContext;
	m_renderer->outputBufferStride = 256;
	m_threadContext = {
		.debugger = 0,
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
		controller->m_pauseMutex.lock();
		if (controller->m_pauseAfterFrame) {
			GBAThreadPause(context);
			controller->m_pauseAfterFrame = false;
		}
		controller->m_pauseMutex.unlock();
		controller->frameAvailable(controller->m_drawContext);
	};
}

GameController::~GameController() {
	if (GBAThreadIsPaused(&m_threadContext)) {
		GBAThreadUnpause(&m_threadContext);
	}
	GBAThreadEnd(&m_threadContext);
	GBAThreadJoin(&m_threadContext);
	delete m_renderer;
}

ARMDebugger* GameController::debugger() {
	return m_threadContext.debugger;
}

void GameController::setDebugger(ARMDebugger* debugger) {
	bool wasPaused = isPaused();
	setPaused(true);
	if (m_threadContext.debugger) {
		GBADetachDebugger(m_threadContext.gba);
	}
	m_threadContext.debugger = debugger;
	if (m_threadContext.debugger) {
		GBAAttachDebugger(m_threadContext.gba, m_threadContext.debugger);
	}
	setPaused(wasPaused);
}

void GameController::loadGame(const QString& path) {
	m_rom = new QFile(path);
	if (!m_rom->open(QIODevice::ReadOnly)) {
		delete m_rom;
		m_rom = 0;
	}

	m_pauseAfterFrame = false;

	m_threadContext.fd = m_rom->handle();
	m_threadContext.fname = path.toLocal8Bit().constData();
	GBAThreadStart(&m_threadContext);
	emit gameStarted(&m_threadContext);
}

bool GameController::isPaused() {
	return GBAThreadIsPaused(&m_threadContext);
}

void GameController::setPaused(bool paused) {
	if (paused == GBAThreadIsPaused(&m_threadContext)) {
		return;
	}
	if (paused) {
		GBAThreadPause(&m_threadContext);
	} else {
		GBAThreadUnpause(&m_threadContext);
	}
}

void GameController::frameAdvance() {
	m_pauseMutex.lock();
	m_pauseAfterFrame = true;
	setPaused(false);
	m_pauseMutex.unlock();
}

void GameController::keyPressed(int key) {
	int mappedKey = 1 << key;
	m_threadContext.activeKeys |= mappedKey;
}

void GameController::keyReleased(int key) {
	int mappedKey = 1 << key;
	m_threadContext.activeKeys &= ~mappedKey;
}
