/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GameController.h"

#include "AudioProcessor.h"
#include "InputController.h"
#include "MultiplayerController.h"
#include "VFileDevice.h"

#include <QDateTime>
#include <QThread>

#include <ctime>

extern "C" {
#include "gba/audio.h"
#include "gba/gba.h"
#include "gba/serialize.h"
#include "gba/sharkport.h"
#include "gba/renderers/video-software.h"
#include "gba/supervisor/config.h"
#include "util/vfs.h"
}

using namespace QGBA;
using namespace std;

const int GameController::LUX_LEVELS[10] = { 5, 11, 18, 27, 42, 62, 84, 109, 139, 183 };

GameController::GameController(QObject* parent)
	: QObject(parent)
	, m_drawContext(new uint32_t[256 * 256])
	, m_threadContext()
	, m_activeKeys(0)
	, m_inactiveKeys(0)
	, m_logLevels(0)
	, m_gameOpen(false)
	, m_audioThread(new QThread(this))
	, m_audioProcessor(AudioProcessor::create())
	, m_pauseAfterFrame(false)
	, m_videoSync(VIDEO_SYNC)
	, m_audioSync(AUDIO_SYNC)
	, m_fpsTarget(-1)
	, m_turbo(false)
	, m_turboForced(false)
	, m_turboSpeed(-1)
	, m_wasPaused(false)
	, m_inputController(nullptr)
	, m_multiplayer(nullptr)
	, m_stateSlot(1)
{
	m_renderer = new GBAVideoSoftwareRenderer;
	GBAVideoSoftwareRendererCreate(m_renderer);
	m_renderer->outputBuffer = (color_t*) m_drawContext;
	m_renderer->outputBufferStride = 256;

	GBACheatDeviceCreate(&m_cheatDevice);

	m_threadContext.state = THREAD_INITIALIZED;
	m_threadContext.debugger = 0;
	m_threadContext.frameskip = 0;
	m_threadContext.bios = 0;
	m_threadContext.renderer = &m_renderer->d;
	m_threadContext.userData = this;
	m_threadContext.rewindBufferCapacity = 0;
	m_threadContext.cheats = &m_cheatDevice;
	m_threadContext.logLevel = GBA_LOG_ALL;

	m_lux.p = this;
	m_lux.sample = [](GBALuminanceSource* context) {
		GameControllerLux* lux = static_cast<GameControllerLux*>(context);
		lux->value = 0xFF - lux->p->m_luxValue;
	};

	m_lux.readLuminance = [](GBALuminanceSource* context) {
		GameControllerLux* lux = static_cast<GameControllerLux*>(context);
		return lux->value;
	};
	setLuminanceLevel(0);

	m_threadContext.startCallback = [](GBAThread* context) {
		GameController* controller = static_cast<GameController*>(context->userData);
		controller->m_audioProcessor->setInput(context);
		context->gba->luminanceSource = &controller->m_lux;
		GBARTCGenericSourceInit(&controller->m_rtc, context->gba);
		context->gba->rtcSource = &controller->m_rtc.d;
		context->gba->rumble = controller->m_inputController->rumble();
		context->gba->rotationSource = controller->m_inputController->rotationSource();
		controller->m_fpsTarget = context->fpsTarget;
		controller->gameStarted(context);
	};

	m_threadContext.cleanCallback = [](GBAThread* context) {
		GameController* controller = static_cast<GameController*>(context->userData);
		controller->gameStopped(context);
	};

	m_threadContext.frameCallback = [](GBAThread* context) {
		GameController* controller = static_cast<GameController*>(context->userData);
		if (controller->m_pauseAfterFrame.testAndSetAcquire(true, false)) {
			GBAThreadPauseFromThread(context);
			controller->gamePaused(&controller->m_threadContext);
		}
		if (GBASyncDrawingFrame(&controller->m_threadContext.sync)) {
			controller->frameAvailable(controller->m_drawContext);
		} else {
			controller->frameAvailable(nullptr);
		}
	};

	m_threadContext.logHandler = [](GBAThread* context, enum GBALogLevel level, const char* format, va_list args) {
		static const char* stubMessage = "Stub software interrupt";
		if (!context) {
			return;
		}
		GameController* controller = static_cast<GameController*>(context->userData);
		if (level == GBA_LOG_STUB && strncmp(stubMessage, format, strlen(stubMessage)) == 0) {
			va_list argc;
			va_copy(argc, args);
			int immediate = va_arg(argc, int);
			va_end(argc);
			controller->unimplementedBiosCall(immediate);
		}
		if (level == GBA_LOG_FATAL) {
			QMetaObject::invokeMethod(controller, "crashGame", Q_ARG(const QString&, QString().vsprintf(format, args)));
		} else if (!(controller->m_logLevels & level)) {
			return;
		}
		QString message(QString().vsprintf(format, args));
		if (level == GBA_LOG_STATUS) {
			controller->statusPosted(message);
		}
		controller->postLog(level, message);
	};

	connect(&m_rewindTimer, &QTimer::timeout, [this]() {
		GBARewind(&m_threadContext, 1);
		emit frameAvailable(m_drawContext);
		emit rewound(&m_threadContext);
	});
	m_rewindTimer.setInterval(100);

	m_audioThread->setObjectName("Audio Thread");
	m_audioThread->start(QThread::TimeCriticalPriority);
	m_audioProcessor->moveToThread(m_audioThread);
	connect(this, SIGNAL(gameStarted(GBAThread*)), m_audioProcessor, SLOT(start()));
	connect(this, SIGNAL(gameStopped(GBAThread*)), m_audioProcessor, SLOT(pause()));
	connect(this, SIGNAL(gamePaused(GBAThread*)), m_audioProcessor, SLOT(pause()));
	connect(this, SIGNAL(gameUnpaused(GBAThread*)), m_audioProcessor, SLOT(start()));
	connect(this, SIGNAL(frameAvailable(const uint32_t*)), this, SLOT(pollEvents()));
}

GameController::~GameController() {
	m_audioThread->quit();
	m_audioThread->wait();
	disconnect();
	clearMultiplayerController();
	closeGame();
	GBACheatDeviceDestroy(&m_cheatDevice);
	delete m_renderer;
	delete[] m_drawContext;
}

void GameController::setMultiplayerController(MultiplayerController* controller) {
	if (controller == m_multiplayer) {
		return;
	}
	clearMultiplayerController();
	m_multiplayer = controller;
	controller->attachGame(this);
}

void GameController::clearMultiplayerController() {
	if (!m_multiplayer) {
		return;
	}
	m_multiplayer->detachGame(this);
	m_multiplayer = nullptr;
}

void GameController::setOverride(const GBACartridgeOverride& override) {
	m_threadContext.override = override;
	m_threadContext.hasOverride = true;
}

void GameController::setOptions(const GBAOptions* opts) {
	setFrameskip(opts->frameskip);
	setAudioSync(opts->audioSync);
	setVideoSync(opts->videoSync);
	setSkipBIOS(opts->skipBios);
	setUseBIOS(opts->useBios);
	setRewind(opts->rewindEnable, opts->rewindBufferCapacity, opts->rewindBufferInterval);
	setVolume(opts->volume);
	setMute(opts->mute);

	threadInterrupt();
	m_threadContext.idleOptimization = opts->idleOptimization;
	threadContinue();
}

#ifdef USE_GDB_STUB
ARMDebugger* GameController::debugger() {
	return m_threadContext.debugger;
}

void GameController::setDebugger(ARMDebugger* debugger) {
	threadInterrupt();
	if (m_threadContext.debugger && GBAThreadIsActive(&m_threadContext)) {
		GBADetachDebugger(m_threadContext.gba);
	}
	m_threadContext.debugger = debugger;
	if (m_threadContext.debugger && GBAThreadIsActive(&m_threadContext)) {
		GBAAttachDebugger(m_threadContext.gba, m_threadContext.debugger);
	}
	threadContinue();
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

void GameController::bootBIOS() {
	closeGame();
	m_fname = QString();
	m_dirmode = false;
	openGame(true);
}

void GameController::openGame(bool biosOnly) {
	if (biosOnly && (!m_useBios || m_bios.isNull())) {
		return;
	}

	m_gameOpen = true;

	m_pauseAfterFrame = false;

	if (m_turbo) {
		m_threadContext.sync.videoFrameWait = false;
		m_threadContext.sync.audioWait = false;
	} else {
		m_threadContext.sync.videoFrameWait = m_videoSync;
		m_threadContext.sync.audioWait = m_audioSync;
	}

	m_threadContext.gameDir = 0;
	m_threadContext.bootBios = biosOnly;
	if (biosOnly) {
		m_threadContext.fname = nullptr;
	} else {
		m_threadContext.fname = strdup(m_fname.toLocal8Bit().constData());
		if (m_dirmode) {
			m_threadContext.gameDir = VDirOpen(m_threadContext.fname);
			m_threadContext.stateDir = m_threadContext.gameDir;
		} else {
			GBAThreadLoadROM(&m_threadContext, m_threadContext.fname);
		}
	}

	if (!m_bios.isNull() && m_useBios) {
		m_threadContext.bios = VFileDevice::open(m_bios, O_RDONLY);
	} else {
		m_threadContext.bios = nullptr;
	}

	if (!m_patch.isNull()) {
		m_threadContext.patch = VFileDevice::open(m_patch, O_RDONLY);
	}

	m_inputController->recalibrateAxes();

	if (!GBAThreadStart(&m_threadContext)) {
		m_gameOpen = false;
		emit gameFailed();
	}
}

void GameController::loadBIOS(const QString& path) {
	if (m_bios == path) {
		return;
	}
	m_bios = path;
	if (m_gameOpen) {
		closeGame();
		openGame();
	}
}

void GameController::yankPak() {
	if (!m_gameOpen) {
		return;
	}
	threadInterrupt();
	GBAYankROM(m_threadContext.gba);
	threadContinue();
}

void GameController::replaceGame(const QString& path) {
	if (!m_gameOpen) {
		return;
	}

	m_fname = path;
	threadInterrupt();
	m_threadContext.fname = strdup(m_fname.toLocal8Bit().constData());
	GBAThreadReplaceROM(&m_threadContext, m_threadContext.fname);
	threadContinue();
}

void GameController::loadPatch(const QString& path) {
	if (m_gameOpen) {
		closeGame();
		m_patch = path;
		openGame();
	} else {
		m_patch = path;
	}
}

void GameController::importSharkport(const QString& path) {
	if (!m_gameOpen) {
		return;
	}
	VFile* vf = VFileDevice::open(path, O_RDONLY);
	if (!vf) {
		return;
	}
	threadInterrupt();
	GBASavedataImportSharkPort(m_threadContext.gba, vf, false);
	threadContinue();
	vf->close(vf);
}

void GameController::exportSharkport(const QString& path) {
	if (!m_gameOpen) {
		return;
	}
	VFile* vf = VFileDevice::open(path, O_WRONLY | O_CREAT | O_TRUNC);
	if (!vf) {
		return;
	}
	threadInterrupt();
	GBASavedataExportSharkPort(m_threadContext.gba, vf);
	threadContinue();
	vf->close(vf);
}

void GameController::closeGame() {
	if (!m_gameOpen) {
		return;
	}
	m_rewindTimer.stop();
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

	for (size_t i = 0; i < GBACheatSetsSize(&m_cheatDevice.cheats); ++i) {
		GBACheatSet* set = *GBACheatSetsGetPointer(&m_cheatDevice.cheats, i);
		GBACheatSetDeinit(set);
		delete set;
	}
	GBACheatSetsClear(&m_cheatDevice.cheats);

	m_gameOpen = false;
	emit gameStopped(&m_threadContext);
}

void GameController::crashGame(const QString& crashMessage) {
	closeGame();
	emit gameCrashed(crashMessage);
	emit gameStopped(&m_threadContext);
}

bool GameController::isPaused() {
	if (!m_gameOpen) {
		return false;
	}
	return GBAThreadIsPaused(&m_threadContext);
}

void GameController::setPaused(bool paused) {
	if (!m_gameOpen || m_rewindTimer.isActive() || paused == GBAThreadIsPaused(&m_threadContext)) {
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
	if (m_rewindTimer.isActive()) {
		return;
	}
	if (m_pauseAfterFrame.testAndSetRelaxed(false, true)) {
		setPaused(false);
	}
}

void GameController::setRewind(bool enable, int capacity, int interval) {
	if (m_gameOpen) {
		threadInterrupt();
		GBARewindSettingsChanged(&m_threadContext, enable ? capacity : 0, enable ? interval : 0);
		threadContinue();
	} else {
		if (enable) {
			m_threadContext.rewindBufferInterval = interval;
			m_threadContext.rewindBufferCapacity = capacity;
		} else {
			m_threadContext.rewindBufferInterval = 0;
			m_threadContext.rewindBufferCapacity = 0;
		}
	}
}

void GameController::rewind(int states) {
	threadInterrupt();
	if (!states) {
		GBARewindAll(&m_threadContext);
	} else {
		GBARewind(&m_threadContext, states);
	}
	threadContinue();
	emit frameAvailable(m_drawContext);
	emit rewound(&m_threadContext);
}

void GameController::startRewinding() {
	if (!m_gameOpen || m_rewindTimer.isActive()) {
		return;
	}
	m_wasPaused = isPaused();
	setPaused(true);
	m_rewindTimer.start();
}

void GameController::stopRewinding() {
	if (!m_rewindTimer.isActive()) {
		return;
	}
	m_rewindTimer.stop();
	setPaused(m_wasPaused);
}

void GameController::keyPressed(int key) {
	int mappedKey = 1 << key;
	m_activeKeys |= mappedKey;
	if (!m_inputController->allowOpposing()) {
		if ((m_activeKeys & 0x30) == 0x30) {
			m_inactiveKeys |= mappedKey ^ 0x30;
			m_activeKeys ^= mappedKey ^ 0x30;
		}
		if ((m_activeKeys & 0xC0) == 0xC0) {
			m_inactiveKeys |= mappedKey ^ 0xC0;
			m_activeKeys ^= mappedKey ^ 0xC0;
		}
	}
	updateKeys();
}

void GameController::keyReleased(int key) {
	int mappedKey = 1 << key;
	m_activeKeys &= ~mappedKey;
	if (!m_inputController->allowOpposing()) {
		if (mappedKey & 0x30) {
			m_activeKeys |= m_inactiveKeys & (0x30 ^ mappedKey);
			m_inactiveKeys &= ~0x30;
		}
		if (mappedKey & 0xC0) {
			m_activeKeys |= m_inactiveKeys & (0xC0 ^ mappedKey);
			m_inactiveKeys &= ~0xC0;
		}
	}
	updateKeys();
}

void GameController::clearKeys() {
	m_activeKeys = 0;
	m_inactiveKeys = 0;
	updateKeys();
}

void GameController::setAudioBufferSamples(int samples) {
	threadInterrupt();
	redoSamples(samples);
	threadContinue();
	QMetaObject::invokeMethod(m_audioProcessor, "setBufferSamples", Q_ARG(int, samples));
}

void GameController::setFPSTarget(float fps) {
	threadInterrupt();
	m_fpsTarget = fps;
	m_threadContext.fpsTarget = fps;
	if (m_turbo && m_turboSpeed > 0) {
		m_threadContext.fpsTarget *= m_turboSpeed;
	}
	redoSamples(m_audioProcessor->getBufferSamples());
	threadContinue();
}

void GameController::setSkipBIOS(bool set) {
	threadInterrupt();
	m_threadContext.skipBios = set;
	threadContinue();
}

void GameController::setUseBIOS(bool use) {
	threadInterrupt();
	m_useBios = use;
	threadContinue();
}

void GameController::loadState(int slot) {
	if (slot > 0) {
		m_stateSlot = slot;
	}
	GBARunOnThread(&m_threadContext, [](GBAThread* context) {
		GameController* controller = static_cast<GameController*>(context->userData);
		if (GBALoadState(context, context->stateDir, controller->m_stateSlot)) {
			controller->frameAvailable(controller->m_drawContext);
			controller->stateLoaded(context);
		}
	});
}

void GameController::saveState(int slot) {
	if (slot > 0) {
		m_stateSlot = slot;
	}
	GBARunOnThread(&m_threadContext, [](GBAThread* context) {
		GameController* controller = static_cast<GameController*>(context->userData);
		GBASaveState(context, context->stateDir, controller->m_stateSlot, true);
	});
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

void GameController::setVolume(int volume) {
	threadInterrupt();
	m_threadContext.volume = volume;
	if (m_gameOpen) {
		m_threadContext.gba->audio.masterVolume = volume;
	}
	threadContinue();
}

void GameController::setMute(bool mute) {
	threadInterrupt();
	m_threadContext.mute = mute;
	if (m_gameOpen) {
		m_threadContext.gba->audio.masterVolume = mute ? 0 : m_threadContext.volume;
	}
	threadContinue();
}

void GameController::setTurbo(bool set, bool forced) {
	if (m_turboForced && !forced) {
		return;
	}
	if (m_turbo == set && m_turboForced == forced) {
		// Don't interrupt the thread if we don't need to
		return;
	}
	m_turbo = set;
	m_turboForced = set && forced;
	enableTurbo();
}

void GameController::setTurboSpeed(float ratio) {
	m_turboSpeed = ratio;
	enableTurbo();
}

void GameController::enableTurbo() {
	threadInterrupt();
	if (!m_turbo) {
		m_threadContext.fpsTarget = m_fpsTarget;
		m_threadContext.sync.audioWait = m_audioSync;
		m_threadContext.sync.videoFrameWait = m_videoSync;
	} else if (m_turboSpeed <= 0) {
		m_threadContext.fpsTarget = m_fpsTarget;
		m_threadContext.sync.audioWait = false;
		m_threadContext.sync.videoFrameWait = false;
	} else {
		m_threadContext.fpsTarget = m_fpsTarget * m_turboSpeed;
		m_threadContext.sync.audioWait = true;
		m_threadContext.sync.videoFrameWait = false;
	}
	redoSamples(m_audioProcessor->getBufferSamples());
	threadContinue();
}

void GameController::setAVStream(GBAAVStream* stream) {
	threadInterrupt();
	m_threadContext.stream = stream;
	if (m_gameOpen) {
		m_threadContext.gba->stream = stream;
	}
	threadContinue();
}

void GameController::clearAVStream() {
	threadInterrupt();
	m_threadContext.stream = nullptr;
	if (m_gameOpen) {
		m_threadContext.gba->stream = nullptr;
	}
	threadContinue();
}

#ifdef USE_PNG
void GameController::screenshot() {
	GBARunOnThread(&m_threadContext, GBAThreadTakeScreenshot);
}
#endif

void GameController::reloadAudioDriver() {
	QMetaObject::invokeMethod(m_audioProcessor, "pause", Qt::BlockingQueuedConnection);
	int samples = m_audioProcessor->getBufferSamples();
	delete m_audioProcessor;
	m_audioProcessor = AudioProcessor::create();
	m_audioProcessor->setBufferSamples(samples);
	m_audioProcessor->moveToThread(m_audioThread);
	connect(this, SIGNAL(gameStarted(GBAThread*)), m_audioProcessor, SLOT(start()));
	connect(this, SIGNAL(gameStopped(GBAThread*)), m_audioProcessor, SLOT(pause()));
	connect(this, SIGNAL(gamePaused(GBAThread*)), m_audioProcessor, SLOT(pause()));
	connect(this, SIGNAL(gameUnpaused(GBAThread*)), m_audioProcessor, SLOT(start()));
	if (isLoaded()) {
		m_audioProcessor->setInput(&m_threadContext);
		QMetaObject::invokeMethod(m_audioProcessor, "start");
	}
}

void GameController::setLuminanceValue(uint8_t value) {
	m_luxValue = value;
	value = std::max<int>(value - 0x16, 0);
	m_luxLevel = 10;
	for (int i = 0; i < 10; ++i) {
		if (value < LUX_LEVELS[i]) {
			m_luxLevel = i;
			break;
		}
	}
	emit luminanceValueChanged(m_luxValue);
}

void GameController::setLuminanceLevel(int level) {
	int value = 0x16;
	level = std::max(0, std::min(10, level));
	if (level > 0) {
		value += LUX_LEVELS[level - 1];
	}
	setLuminanceValue(value);
}

void GameController::setRealTime() {
	m_rtc.override = GBARTCGenericSource::RTC_NO_OVERRIDE;
}

void GameController::setFixedTime(const QDateTime& time) {
	m_rtc.override = GBARTCGenericSource::RTC_FIXED;
	m_rtc.value = time.toMSecsSinceEpoch() / 1000;
}

void GameController::setFakeEpoch(const QDateTime& time) {
	m_rtc.override = GBARTCGenericSource::RTC_FAKE_EPOCH;
	m_rtc.value = time.toMSecsSinceEpoch() / 1000;
}

void GameController::updateKeys() {
	int activeKeys = m_activeKeys;
	activeKeys |= m_activeButtons;
	activeKeys &= ~m_inactiveKeys;
	m_threadContext.activeKeys = activeKeys;
}

void GameController::redoSamples(int samples) {
#if RESAMPLE_LIBRARY != RESAMPLE_BLIP_BUF
	float sampleRate = 0x8000;
	float ratio;
	if (m_threadContext.gba) {
		sampleRate = m_threadContext.gba->audio.sampleRate;
	}
	ratio = GBAAudioCalculateRatio(sampleRate, m_threadContext.fpsTarget, 44100);
	m_threadContext.audioBuffers = ceil(samples / ratio);
#else
	m_threadContext.audioBuffers = samples;
#endif
	if (m_threadContext.gba) {
		GBAAudioResizeBuffer(&m_threadContext.gba->audio, m_threadContext.audioBuffers);
	}
	QMetaObject::invokeMethod(m_audioProcessor, "inputParametersChanged");
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

void GameController::pollEvents() {
	if (!m_inputController) {
		return;
	}

	m_activeButtons = m_inputController->pollEvents();
	updateKeys();
}
