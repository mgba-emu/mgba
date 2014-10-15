#include "GameController.h"

#include "AudioProcessor.h"

#include <QThread>

extern "C" {
#include "gba.h"
#include "gba-audio.h"
#include "renderers/video-software.h"
#include "util/vfs.h"
}

using namespace QGBA;

GameController::GameController(QObject* parent)
	: QObject(parent)
	, m_drawContext(new uint32_t[256 * 256])
	, m_activeKeys(0)
	, m_rom(nullptr)
	, m_audioThread(new QThread(this))
	, m_audioProcessor(new AudioProcessor)
{
	m_renderer = new GBAVideoSoftwareRenderer;
	GBAVideoSoftwareRendererCreate(m_renderer);
	m_renderer->outputBuffer = (color_t*) m_drawContext;
	m_renderer->outputBufferStride = 256;
	m_threadContext = {
		.state = THREAD_INITIALIZED,
		.debugger = 0,
		.frameskip = 0,
		.bios = 0,
		.renderer = &m_renderer->d,
		.userData = this,
		.rewindBufferCapacity = 0,
		.logLevel = -1,
	};

	GBAInputMapInit(&m_threadContext.inputMap);

#ifdef BUILD_SDL
	SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);
	m_sdlEvents.bindings = &m_threadContext.inputMap;
	GBASDLInitEvents(&m_sdlEvents);
	SDL_JoystickEventState(SDL_QUERY);
#endif

	m_threadContext.startCallback = [] (GBAThread* context) {
		GameController* controller = static_cast<GameController*>(context->userData);
		controller->m_audioProcessor->setInput(context);
		controller->gameStarted(context);
	};

	m_threadContext.cleanCallback = [] (GBAThread* context) {
		GameController* controller = static_cast<GameController*>(context->userData);
		controller->gameStopped(context);
	};

	m_threadContext.frameCallback = [] (GBAThread* context) {
		GameController* controller = static_cast<GameController*>(context->userData);
		controller->m_pauseMutex.lock();
		if (controller->m_pauseAfterFrame) {
			GBAThreadPauseFromThread(context);
			controller->m_pauseAfterFrame = false;
			controller->gamePaused(&controller->m_threadContext);
		}
		controller->m_pauseMutex.unlock();
		controller->frameAvailable(controller->m_drawContext);
	};

	m_threadContext.logHandler = [] (GBAThread* context, enum GBALogLevel level, const char* format, va_list args) {
		GameController* controller = static_cast<GameController*>(context->userData);
		controller->postLog(level, QString().vsprintf(format, args));
	};

	m_audioThread->start();
	m_audioProcessor->moveToThread(m_audioThread);
	connect(this, SIGNAL(gameStarted(GBAThread*)), m_audioProcessor, SLOT(start()));
	connect(this, SIGNAL(gameStopped(GBAThread*)), m_audioProcessor, SLOT(pause()));
	connect(this, SIGNAL(gamePaused(GBAThread*)), m_audioProcessor, SLOT(pause()));
	connect(this, SIGNAL(gameUnpaused(GBAThread*)), m_audioProcessor, SLOT(start()));

#ifdef BUILD_SDL
	connect(this, SIGNAL(frameAvailable(const uint32_t*)), this, SLOT(testSDLEvents()));
#endif
}

GameController::~GameController() {
	m_audioThread->quit();
	m_audioThread->wait();
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
	if (m_threadContext.debugger && GBAThreadHasStarted(&m_threadContext)) {
		GBADetachDebugger(m_threadContext.gba);
	}
	m_threadContext.debugger = debugger;
	if (m_threadContext.debugger && GBAThreadHasStarted(&m_threadContext)) {
		GBAAttachDebugger(m_threadContext.gba, m_threadContext.debugger);
	}
	setPaused(wasPaused);
}

void GameController::loadGame(const QString& path) {
	closeGame();
	m_threadContext.sync.videoFrameWait = 0;
	m_threadContext.sync.audioWait = 1;
	m_rom = new QFile(path);
	if (!m_rom->open(QIODevice::ReadOnly)) {
		delete m_rom;
		m_rom = nullptr;
	}

	m_pauseAfterFrame = false;

	m_threadContext.rom = VFileFromFD(m_rom->handle());
	m_threadContext.fname = strdup(path.toLocal8Bit().constData());

	GBAThreadStart(&m_threadContext);
}

void GameController::closeGame() {
	if (!m_rom) {
		return;
	}
	GBAThreadEnd(&m_threadContext);
	GBAThreadJoin(&m_threadContext);
	if (m_threadContext.fname) {
		free(const_cast<char*>(m_threadContext.fname));
		m_threadContext.fname = nullptr;
	}
	if (m_rom) {
		m_rom->close();
		delete m_rom;
		m_rom = nullptr;
	}
	emit gameStopped(&m_threadContext);
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
		emit gamePaused(&m_threadContext);
	} else {
		GBAThreadUnpause(&m_threadContext);
		emit gameUnpaused(&m_threadContext);
	}
}

void GameController::reset() {
	GBAThreadReset(&m_threadContext);
}

void GameController::frameAdvance() {
	m_pauseMutex.lock();
	m_pauseAfterFrame = true;
	setPaused(false);
	m_pauseMutex.unlock();
}

void GameController::keyPressed(int key) {
	int mappedKey = 1 << key;
	m_activeKeys |= mappedKey;
	updateKeys();
}

void GameController::keyReleased(int key) {
	int mappedKey = 1 << key;
	m_activeKeys &= ~mappedKey;
	updateKeys();
}

void GameController::setAudioBufferSamples(int samples) {
	GBAThreadInterrupt(&m_threadContext);
	m_threadContext.audioBuffers = samples;
	GBAAudioResizeBuffer(&m_threadContext.gba->audio, samples);
	GBAThreadContinue(&m_threadContext);
	QMetaObject::invokeMethod(m_audioProcessor, "setBufferSamples", Q_ARG(int, samples));
}

void GameController::setFPSTarget(float fps) {
	GBAThreadInterrupt(&m_threadContext);
	m_threadContext.fpsTarget = fps;
	GBAThreadContinue(&m_threadContext);
	QMetaObject::invokeMethod(m_audioProcessor, "inputParametersChanged");
}

void GameController::updateKeys() {
	int activeKeys = m_activeKeys;
#ifdef BUILD_SDL
	activeKeys |= m_activeButtons;
#endif
	m_threadContext.activeKeys = activeKeys;
}

#ifdef BUILD_SDL
void GameController::testSDLEvents() {
	SDL_Joystick* joystick = m_sdlEvents.joystick;
	SDL_JoystickUpdate();
	int numButtons = SDL_JoystickNumButtons(joystick);
	m_activeButtons = 0;
	int i;
	for (i = 0; i < numButtons; ++i) {
		GBAKey key = GBAInputMapKey(&m_threadContext.inputMap, SDL_BINDING_BUTTON, i);
		if (key == GBA_KEY_NONE) {
			continue;
		}
		if (SDL_JoystickGetButton(joystick, i)) {
			m_activeButtons |= 1 << key;
		}
	}
	int numHats = SDL_JoystickNumHats(joystick);
	for (i = 0; i < numHats; ++i) {
		int hat = SDL_JoystickGetHat(joystick, i);
		if (hat & SDL_HAT_UP) {
			m_activeButtons |= 1 << GBA_KEY_UP;
		}
		if (hat & SDL_HAT_LEFT) {
			m_activeButtons |= 1 << GBA_KEY_LEFT;
		}
		if (hat & SDL_HAT_DOWN) {
			m_activeButtons |= 1 << GBA_KEY_DOWN;
		}
		if (hat & SDL_HAT_RIGHT) {
			m_activeButtons |= 1 << GBA_KEY_RIGHT;
		}
	}
	updateKeys();
}
#endif
