/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

#include "log/log.h"
#include "core.h"

Core::Core() {
    s_audioRateOwner.store(this, std::memory_order_release);
}

Core::~Core() {
    Core * expected = this;
    s_audioRateOwner.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel, std::memory_order_acquire);
}

bool Core::init() {
	LOGI("Core::init");
	return true;
}

void Core::shutdown() {
	unloadRom();
	m_audioPlayer.stop();
}

bool Core::validateRom(int romFd) {
	if (m_core != nullptr) {
		unloadRom();
	}

	VFile* vf = VFileFromFD(romFd);
	if (!vf) {
		LOGE("VFileFromConstMemory failed");
		return false;
	}

	m_core = mCoreFindVF(vf);
	if (!m_core) {
		LOGE("mCoreFindVF failed to identify ROM type");
		vf->close(vf);
		return false;
	}

	if (!m_core->init(m_core)) {
		LOGE("mCore init failed");
		m_core->deinit(m_core);
		m_core = nullptr;
		return false;
	}

	if (!m_core->loadROM(m_core, vf)) {
		LOGE("mCore loadROM failed");
		m_core->deinit(m_core);
		m_core = nullptr;
		return false;
	}

	m_gameTitle.clear();
	NoIntroMetadata metadata;

	const uint32_t crc32 = getRomCRC32();

	if (noIntroLookupCRC32(crc32, metadata) && !metadata.name.empty()) {
		m_gameTitle = metadata.name;

		LOGD("No-Intro match: title='%s' rom='%s' crc=%08X", metadata.name.c_str(), metadata.romName.c_str(),
		     metadata.crc32);
	} else {
		LOGD("No-Intro: no match for CRC32=%08X", crc32);
	}

	return true;
}

bool Core::loadRom(int romFd, bool rtcEnable) {
	if (m_core != nullptr) {
		unloadRom();
	}

	VFile* vf = VFileFromFD(romFd);
	if (!vf) {
		LOGE("VFileFromConstMemory failed");
		return false;
	}

	m_core = mCoreFindVF(vf);
	if (!m_core) {
		LOGE("mCoreFindVF failed to identify ROM type");
		vf->close(vf);
		return false;
	}

	if (!m_core->init(m_core)) {
		LOGE("mCore init failed");
		m_core->deinit(m_core);
		m_core = nullptr;
		return false;
	}

	mCoreInitConfig(m_core, nullptr);

	if (!rtcEnable) {
		m_core->rtc.override = RTC_FIXED;
	}

	LOGI("Config Applied -> Key: '%s' = %d", "hw.rtc", rtcEnable ? 1 : 0);

	unsigned width, height;
	m_core->baseVideoSize(m_core, &width, &height);
	m_width = static_cast<int>(width);
	m_height = static_cast<int>(height);

	m_videoBuffer.assign(static_cast<size_t>(m_width) * m_height, 0);

	m_core->setVideoBuffer(m_core, m_videoBuffer.data(), static_cast<size_t>(m_width));

	m_sampleRate = 0;

	m_avStream = { };
	m_avStream.audioRateChanged = &Core::audioRateChangedThunk;
	m_core->setAVStream(m_core, &m_avStream);

	if (!m_core->loadROM(m_core, vf)) {
		LOGE("mCore loadROM failed");
		m_core->deinit(m_core);
		m_core = nullptr;
		return false;
	}

	m_coreSampleRate = static_cast<int>(m_core->audioSampleRate(m_core));
	if (m_coreSampleRate <= 0) {
		LOGE("Invalid native audio sample rate: %d", m_coreSampleRate);
		m_core->deinit(m_core);
		m_core = nullptr;
		return false;
	}

	configureCoreAudioBuffer(m_coreSampleRate);

	if (m_audioPlayer.hasStream()) {
		m_audioPlayer.stop();
	}

	constexpr int kPreferredOutputRate = 48000;
	if (!m_audioPlayer.start(kPreferredOutputRate, kAudioRingBufferFrames)) {
		LOGE("Failed to start Oboe audio playback, continuing without audio");
	}

	m_sampleRate = m_audioPlayer.getSampleRate();
	if (m_sampleRate <= 0) {
		LOGE("Oboe returned invalid sample rate: %d", m_sampleRate);
		m_audioPlayer.stop();
		m_core->deinit(m_core);
		m_core = nullptr;
		return false;
	}

	mAudioBufferInit(&m_resampledAudio, kResampledAudioBufferFrames, 2);
	mAudioResamplerInit(&m_audioResampler, mINTERPOLATOR_SINC);
	mAudioResamplerSetSource(&m_audioResampler, m_core->getAudioBuffer(m_core), static_cast<double>(m_coreSampleRate),
	                         true);
	mAudioResamplerSetDestination(&m_audioResampler, &m_resampledAudio, static_cast<double>(m_sampleRate));

	m_resamplerInitialized = true;
	m_pendingAudioRate.store(0, std::memory_order_release);

	LOGI("ROM loaded: %dx%d coreAudioRate=%d outputRate=%d", m_width, m_height, m_coreSampleRate, m_sampleRate);

	m_core->reset(m_core);
	resetAudioPipeline();
	return true;
}

void Core::unloadRom() {
	m_audioPlayer.stop();

	if (m_resamplerInitialized) {
		mAudioResamplerDeinit(&m_audioResampler);
		mAudioBufferDeinit(&m_resampledAudio);
		m_resamplerInitialized = false;
	}

	if (m_core) {
		m_core->unloadROM(m_core);
		m_core->deinit(m_core);
		m_core = nullptr;
	}

	m_pendingAudioRate.store(0, std::memory_order_release);
	m_coreSampleRate = 0;
	m_sampleRate = 0;
	m_videoBuffer.clear();
}

void Core::reset() {
	if (m_core) {
		m_pendingAudioRate.store(0, std::memory_order_release);
		m_core->reset(m_core);
		m_coreSampleRate = static_cast<int>(m_core->audioSampleRate(m_core));
		configureCoreAudioBuffer(m_coreSampleRate);
		if (m_resamplerInitialized) {
			resetAudioPipeline();
		}
	}
}

void Core::runFrame() {
	if (!m_core || !m_resamplerInitialized)
		return;

	m_core->runFrame(m_core);

	applyPendingAudioRateChange();

	for (size_t i = 0; i < kMaxAudioProcessIterations; ++i) {
		if (m_audioPlayer.freeFrames() == 0)
			break;

		const size_t sourceBefore = mAudioBufferAvailable(m_core->getAudioBuffer(m_core));
		const size_t outputBefore = mAudioBufferAvailable(&m_resampledAudio);
		const size_t produced = mAudioResamplerProcess(&m_audioResampler);

		drainResampledAudio();

		const size_t sourceAfter = mAudioBufferAvailable(m_core->getAudioBuffer(m_core));
		const size_t outputAfter = mAudioBufferAvailable(&m_resampledAudio);

		if (produced == 0 && sourceBefore == sourceAfter && outputBefore == outputAfter) {
			break;
		}
	}
}

const uint32_t* Core::getVideoBuffer() {
	return reinterpret_cast<const uint32_t*>(m_videoBuffer.data());
}

int Core::getWidth() const {
	return m_width;
}
int Core::getHeight() const {
	return m_height;
}

void Core::setKeys(uint16_t keyMask) {
	if (m_core)
		m_core->setKeys(m_core, static_cast<uint32_t>(keyMask));
}

bool Core::saveState(uint8_t* outBuffer, size_t bufferSize, size_t* outWritten) {
	if (!m_core) {
		*outWritten = 0;
		return false;
	}
	VFile* vf = VFileFromMemory(outBuffer, bufferSize);
	if (!vf) {
		*outWritten = 0;
		return false;
	}

	bool ok = mCoreSaveStateNamed(m_core, vf, SAVESTATE_SAVEDATA | SAVESTATE_SCREENSHOT);
	*outWritten = ok ? static_cast<size_t>(vf->seek(vf, 0, SEEK_CUR)) : 0;
	vf->close(vf);
	return ok;
}

bool Core::loadState(const uint8_t* data, size_t size) {
	if (!m_core)
		return false;
	VFile* vf = VFileFromConstMemory(data, size);
	if (!vf)
		return false;
	bool ok = mCoreLoadStateNamed(m_core, vf, SAVESTATE_SAVEDATA | SAVESTATE_SCREENSHOT);
	vf->close(vf);
	return ok;
}

bool Core::loadSaveData(const uint8_t* data, size_t size) {
	if (!m_core)
		return false;
	bool ok = m_core->savedataRestore(m_core, data, size, true);
	if (!ok) {
		LOGE("savedataRestore failed (size=%zu)", size);
	}
	return ok;
}

std::vector<uint8_t> Core::exportSaveData() {
	if (!m_core)
		return { };
	void* sram = nullptr;
	size_t size = m_core->savedataClone(m_core, &sram);
	if (size == 0 || sram == nullptr) {
		return { };
	}

	std::vector<uint8_t> result(static_cast<uint8_t*>(sram), static_cast<uint8_t*>(sram) + size);
	free(sram);
	return result;
}

std::string Core::getGameTitle() {
	return m_gameTitle;
}

std::string Core::getGameCode() {
	if (!m_core)
		return "";
	mGameInfo info { };
	m_core->getGameInfo(m_core, &info);
	info.code[4] = '\0';
	return { info.code };
}

int Core::getPlatform() {
	if (!m_core)
		return PLATFORM_UNKNOWN;

	int basePlatform = m_core->platform(m_core);

	if (basePlatform == mPLATFORM_GBA) {
		return PLATFORM_GBA;
	}

	if (basePlatform == mPLATFORM_GB) {
		auto* gbCore = reinterpret_cast<struct GB*>(m_core->board);
		if (!gbCore)
			return PLATFORM_GB;

		switch (gbCore->model) {
		case GB_MODEL_CGB:
			return PLATFORM_GBC;
		case GB_MODEL_SGB:
			return PLATFORM_SGB;
		default:
			return PLATFORM_GB;
		}
	}

	return PLATFORM_UNKNOWN;
}

bool Core::loadBios(const uint8_t* data, size_t size) {
	if (!m_core) {
		LOGE("loadBios called before loadRom");
		return false;
	}

	VFile* vf = VFileFromConstMemory(data, size);
	if (!vf) {
		LOGE("VFileFromConstMemory failed for BIOS data");
		return false;
	}

	bool ok = m_core->loadBIOS(m_core, vf, 0);
	if (!ok) {
		LOGE("mCore loadBIOS rejected the file (size=%zu)", size);
		vf->close(vf);
	}
	return ok;
}

void Core::setAudioMuted(bool mute) {
	m_audioPlayer.setMuted(mute);
}

void Core::audioRateChangedThunk(struct mAVStream*, unsigned rate) {
	Core* self = s_audioRateOwner.load(std::memory_order_acquire);
	if (self) {
		self->m_pendingAudioRate.store(static_cast<int>(rate), std::memory_order_release);
	}
}

void Core::configureCoreAudioBuffer(int sourceRate) {
	if (!m_core || sourceRate <= 0)
		return;

	const double samplesPerFrame = static_cast<double>(sourceRate) * static_cast<double>(m_core->frameCycles(m_core)) /
	    static_cast<double>(m_core->frequency(m_core));

	auto bufferFrames = static_cast<size_t>(std::ceil(samplesPerFrame)) * 2;
	bufferFrames = std::max<size_t>(bufferFrames, 2);
	bufferFrames = std::min<size_t>(bufferFrames, kMaxCoreAudioBufferFrames);
	m_core->setAudioBufferSize(m_core, bufferFrames);

	LOGI("Core audio buffer: sourceRate=%d samplesPerFrame=%.6f frames=%zu", sourceRate, samplesPerFrame, bufferFrames);
}

void Core::applyPendingAudioRateChange() {
	if (!m_resamplerInitialized || !m_core)
		return;

	const int newRate = m_pendingAudioRate.exchange(0, std::memory_order_acq_rel);
	if (newRate <= 0 || newRate == m_coreSampleRate)
		return;

	const int oldRate = m_coreSampleRate;
	m_coreSampleRate = newRate;
	configureCoreAudioBuffer(newRate);
	resetAudioPipeline();

	LOGI("native audio rate changed: %d -> %d, outputRate=%d", oldRate, newRate, m_sampleRate);
}

void Core::resetAudioPipeline() {
	if (!m_core || !m_resamplerInitialized)
		return;

	mAudioBufferClear(&m_resampledAudio);
	mAudioResamplerDeinit(&m_audioResampler);
	mAudioResamplerInit(&m_audioResampler, mINTERPOLATOR_SINC);
	mAudioResamplerSetSource(&m_audioResampler, m_core->getAudioBuffer(m_core), static_cast<double>(m_coreSampleRate),
	                         true);

	mAudioResamplerSetDestination(&m_audioResampler, &m_resampledAudio, static_cast<double>(m_sampleRate));
}

void Core::drainResampledAudio() {
	if (!m_resamplerInitialized)
		return;

	const size_t available = mAudioBufferAvailable(&m_resampledAudio);
	const size_t freeFrames = m_audioPlayer.freeFrames();
	const size_t frames = std::min({ available, freeFrames, kAudioDrainFrames });

	if (frames == 0)
		return;

	int16_t audioBuf[kAudioDrainFrames * 2];
	const size_t read = mAudioBufferRead(&m_resampledAudio, audioBuf, frames);
	if (read == 0)
		return;

	const size_t written = m_audioPlayer.write(audioBuf, read);
	if (written != read) {
		LOGE("audio output mismatch: resampled=%zu written=%zu", read, written);
	}
}

uint32_t Core::getRomCRC32() const {
	if (!m_core) {
		return 0;
	}

	uint32_t crc32 = 0;
	m_core->checksum(m_core, &crc32, mCHECKSUM_CRC32);

	return crc32;
}

