#include "GameController.h"

#include "AudioProcessor.h"

#include <QThread>

extern "C" {
#include "gba.h"
#include "gba-audio.h"
#include "gba-serialize.h"
#include "renderers/video-software.h"
#include "util/vfs.h"
}

using namespace QGBA;

GameController::GameController(QObject* parent)
	: QObject(parent)
	, m_drawContext(new uint32_t[256 * 256])
	, m_threadContext()
	, m_activeKeys(0)
	, m_gameOpen(false)
	, m_audioThread(new QThread(this))
	, m_audioProcessor(new AudioProcessor)
	, m_videoSync(VIDEO_SYNC)
	, m_audioSync(AUDIO_SYNC)
	, m_turbo(false)
	, m_turboForced(false)
{
	m_renderer = new GBAVideoSoftwareRenderer;
	GBAVideoSoftwareRendererCreate(m_renderer);
	m_renderer->outputBuffer = (color_t*) m_drawContext;
	m_renderer->outputBufferStride = 256;
	m_threadContext.state = THREAD_INITIALIZED;
	m_threadContext.debugger = 0;
	m_threadContext.frameskip = 0;
	m_threadContext.bios = 0;
	m_threadContext.renderer = &m_renderer->d;
	m_threadContext.userData = this;
	m_threadContext.rewindBufferCapacity = 0;
	m_threadContext.logLevel = -1;

	GBAInputMapInit(&m_threadContext.inputMap);

#ifdef BUILD_SDL
	SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_NOPARACHUTE);
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

	m_audioThread->start(QThread::TimeCriticalPriority);
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
	disconnect();
	closeGame();
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

void GameController::loadGame(const QString& path, bool dirmode) {
	closeGame();
	if (!dirmode) {
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly)) {
			return;
		}
		file.close();
	}

	m_fname = path;
	m_dirmode = dirmode;
	openGame();
}

void GameController::openGame() {
	m_gameOpen = true;

	m_pauseAfterFrame = false;

	if (m_turbo) {
		m_threadContext.sync.videoFrameWait = false;
		m_threadContext.sync.audioWait = false;
	} else {
		m_threadContext.sync.videoFrameWait = m_videoSync;
		m_threadContext.sync.audioWait = m_audioSync;
	}

	m_threadContext.fname = strdup(m_fname.toLocal8Bit().constData());
	if (m_dirmode) {
		m_threadContext.gameDir = VDirOpen(m_threadContext.fname);
		m_threadContext.stateDir = m_threadContext.gameDir;
	} else {
		m_threadContext.rom = VFileOpen(m_threadContext.fname, O_RDONLY);
#if ENABLE_LIBZIP
		m_threadContext.gameDir = VDirOpenZip(m_threadContext.fname, 0);
#endif
	}

	if (!m_bios.isNull()) {
		m_threadContext.bios = VFileOpen(m_bios.toLocal8Bit().constData(), O_RDONLY);
	}

	if (!m_patch.isNull()) {
		m_threadContext.patch = VFileOpen(m_patch.toLocal8Bit().constData(), O_RDONLY);
	}

	if (!GBAThreadStart(&m_threadContext)) {
		m_gameOpen = false;
	}
}

void GameController::loadBIOS(const QString& path) {
	m_bios = path;
	if (m_gameOpen) {
		closeGame();
		openGame();
	}
}

void GameController::loadPatch(const QString& path) {
	m_patch = path;
	if (m_gameOpen) {
		closeGame();
		openGame();
	}
}

void GameController::closeGame() {
	if (!m_gameOpen) {
		return;
	}
	GBAThreadEnd(&m_threadContext);
	GBAThreadJoin(&m_threadContext);
	if (m_threadContext.fname) {
		free(const_cast<char*>(m_threadContext.fname));
		m_threadContext.fname = nullptr;
	}

	m_patch = QString();

	m_gameOpen = false;
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

void GameController::loadState(int slot) {
	GBAThreadInterrupt(&m_threadContext);
	GBALoadState(m_threadContext.gba, m_threadContext.stateDir, slot);
	GBAThreadContinue(&m_threadContext);
	emit stateLoaded(&m_threadContext);
	emit frameAvailable(m_drawContext);
}

void GameController::saveState(int slot) {
	GBAThreadInterrupt(&m_threadContext);
	GBASaveState(m_threadContext.gba, m_threadContext.stateDir, slot, true);
	GBAThreadContinue(&m_threadContext);
}

void GameController::setVideoSync(bool set) {
	m_videoSync = set;
	if (!m_turbo && m_gameOpen) {
		GBAThreadInterrupt(&m_threadContext);
		m_threadContext.sync.videoFrameWait = set;
		GBAThreadContinue(&m_threadContext);
	}
}

void GameController::setAudioSync(bool set) {
	m_audioSync = set;
	if (!m_turbo && m_gameOpen) {
		GBAThreadInterrupt(&m_threadContext);
		m_threadContext.sync.audioWait = set;
		GBAThreadContinue(&m_threadContext);
	}
}

void GameController::setFrameskip(int skip) {
	m_threadContext.frameskip = skip;
}

void GameController::setTurbo(bool set, bool forced) {
	if (m_turboForced && !forced) {
		return;
	}
	m_turbo = set;
	if (set) {
		m_turboForced = forced;
	} else {
		m_turboForced = false;
	}
	if (m_gameOpen) {
		GBAThreadInterrupt(&m_threadContext);
		m_threadContext.sync.audioWait = set ? false : m_audioSync;
		m_threadContext.sync.videoFrameWait = set ? false : m_videoSync;
		GBAThreadContinue(&m_threadContext);
	}
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
