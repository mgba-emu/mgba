/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GameController.h"

#include "AudioProcessor.h"
#include "InputController.h"

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
	, m_logLevels(0)
	, m_gameOpen(false)
	, m_audioThread(new QThread(this))
	, m_audioProcessor(AudioProcessor::create())
	, m_videoSync(VIDEO_SYNC)
	, m_audioSync(AUDIO_SYNC)
	, m_turbo(false)
	, m_turboForced(false)
	, m_inputController(nullptr)
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
		if (level == GBA_LOG_FATAL) {
			MutexLock(&controller->m_threadContext.stateMutex);
			controller->m_threadContext.state = THREAD_EXITING;
			MutexUnlock(&controller->m_threadContext.stateMutex);
			QMetaObject::invokeMethod(controller, "crashGame", Q_ARG(const QString&, QString().vsprintf(format, args)));
		} else if (!(controller->m_logLevels & level)) {
			return;
		}
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
	disconnect();
	closeGame();
	delete m_renderer;
	delete[] m_drawContext;
}

#ifdef USE_GDB_STUB
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
#endif

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
	if (GBAThreadIsPaused(&m_threadContext)) {
		GBAThreadUnpause(&m_threadContext);
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

void GameController::crashGame(const QString& crashMessage) {
	closeGame();
	emit gameCrashed(crashMessage);
}

bool GameController::isPaused() {
	if (!m_gameOpen) {
		return false;
	}
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

void GameController::threadInterrupt() {
	if (m_gameOpen) {
		GBAThreadInterrupt(&m_threadContext);
	}
}

void GameController::threadContinue() {
	if (m_gameOpen) {
		GBAThreadContinue(&m_threadContext);
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
	m_activeKeys |= mappedKey;
	updateKeys();
}

void GameController::keyReleased(int key) {
	int mappedKey = 1 << key;
	m_activeKeys &= ~mappedKey;
	updateKeys();
}

void GameController::clearKeys() {
	m_activeKeys = 0;
	updateKeys();
}

void GameController::setAudioBufferSamples(int samples) {
	if (m_gameOpen) {
		threadInterrupt();
		m_threadContext.audioBuffers = samples;
		GBAAudioResizeBuffer(&m_threadContext.gba->audio, samples);
		threadContinue();
	} else {
		m_threadContext.audioBuffers = samples;

	}
	QMetaObject::invokeMethod(m_audioProcessor, "setBufferSamples", Q_ARG(int, samples));
}

void GameController::setFPSTarget(float fps) {
	threadInterrupt();
	m_threadContext.fpsTarget = fps;
	threadContinue();
	QMetaObject::invokeMethod(m_audioProcessor, "inputParametersChanged");
}

void GameController::loadState(int slot) {
	threadInterrupt();
	GBALoadState(m_threadContext.gba, m_threadContext.stateDir, slot);
	threadContinue();
	emit stateLoaded(&m_threadContext);
	emit frameAvailable(m_drawContext);
}

void GameController::saveState(int slot) {
	threadInterrupt();
	GBASaveState(m_threadContext.gba, m_threadContext.stateDir, slot, true);
	threadContinue();
}

void GameController::setVideoSync(bool set) {
	m_videoSync = set;
	if (!m_turbo) {
		threadInterrupt();
		m_threadContext.sync.videoFrameWait = set;
		threadContinue();
	}
}

void GameController::setAudioSync(bool set) {
	m_audioSync = set;
	if (!m_turbo) {
		threadInterrupt();
		m_threadContext.sync.audioWait = set;
		threadContinue();
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
	threadInterrupt();
	m_threadContext.sync.audioWait = set ? false : m_audioSync;
	m_threadContext.sync.videoFrameWait = set ? false : m_videoSync;
	threadContinue();
}

void GameController::setAVStream(GBAAVStream* stream) {
	threadInterrupt();
	m_threadContext.stream = stream;
	threadContinue();
}

void GameController::clearAVStream() {
	threadInterrupt();
	m_threadContext.stream = nullptr;
	threadContinue();
}

void GameController::updateKeys() {
	int activeKeys = m_activeKeys;
#ifdef BUILD_SDL
	activeKeys |= m_activeButtons;
#endif
	m_threadContext.activeKeys = activeKeys;
}

void GameController::setLogLevel(int levels) {
	threadInterrupt();
	m_logLevels = levels;
	threadContinue();
}

void GameController::enableLogLevel(int levels) {
	threadInterrupt();
	m_logLevels |= levels;
	threadContinue();
}

void GameController::disableLogLevel(int levels) {
	threadInterrupt();
	m_logLevels &= ~levels;
	threadContinue();
}

#ifdef BUILD_SDL
void GameController::testSDLEvents() {
	if (!m_inputController) {
		return;
	}

	m_activeButtons = m_inputController->testSDLEvents();
	updateKeys();
}
#endif
