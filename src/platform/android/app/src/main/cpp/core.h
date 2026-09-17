/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

#pragma once

#include "audio/oboe_audio_player.h"
#include "no_intro_parser.h"

#include <cstdint>
#include <cstddef>
#include <string>
#include <algorithm>
#include <android/log.h>
#include <atomic>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
#include "mgba/core/core.h"
#include "mgba/gb/interface.h"
#include <mgba-util/audio-buffer.h>
#include <mgba-util/audio-resampler.h>
#include <mgba-util/vfs.h>
#include <mgba/core/interface.h>
#include <mgba/core/serialize.h>
#include <mgba/core/version.h>
#include <mgba/internal/gb/gb.h>
}

enum class GbaKey : uint16_t {
    A      = 1 << 0,
    B      = 1 << 1,
    SELECT = 1 << 2,
    START  = 1 << 3,
    RIGHT  = 1 << 4,
    LEFT   = 1 << 5,
    UP     = 1 << 6,
    DOWN   = 1 << 7,
    R      = 1 << 8,
    L      = 1 << 9,
};

class Core {
public:
	Core();
	~Core();

	static Core* create() {
		return new Core();
	}

    static bool init();
    void shutdown();

    bool loadRom(int romFd, bool rtcEnable);
    bool validateRom(int romFd);
    void unloadRom();
    void reset();

    void runFrame();

    const uint32_t* getVideoBuffer();
    [[nodiscard]] virtual int getWidth() const;
    [[nodiscard]] virtual int getHeight() const;

    void setKeys(uint16_t keyMask);

    bool saveState(uint8_t* outBuffer, size_t bufferSize, size_t* outWritten);
    bool loadState(const uint8_t* data, size_t size);

    bool loadSaveData(const uint8_t* data, size_t size);
    std::vector<uint8_t> exportSaveData();

    std::string getGameTitle();
    std::string getGameCode();

	int getPlatform();

    bool loadBios(const uint8_t* data, size_t size);
    void setAudioMuted(bool mute);

private:
	mCore* m_core = nullptr;
	OboeAudioPlayer m_audioPlayer;
	std::vector<uint32_t> m_videoBuffer;
	// only populated and used when color_t is 16-bit; empty and unused otherwise.
	std::vector<uint32_t> m_expandedBuffer;
	int m_width = 0;
	int m_height = 0;
	int m_coreSampleRate = 0;
	int m_sampleRate = 0;
	mAudioResampler m_audioResampler { };
	mAudioBuffer m_resampledAudio { };
	mAVStream m_avStream { };
	std::atomic<int> m_pendingAudioRate { 0 };
	bool m_resamplerInitialized = false;

	static constexpr size_t kMaxCoreAudioBufferFrames = 0x4000;
	static constexpr size_t kResampledAudioBufferFrames = 1024;
	static constexpr size_t kAudioDrainFrames = 1024;
	static constexpr size_t kMaxAudioProcessIterations = 2;
	static constexpr size_t kAudioRingBufferFrames = 4096;

	uint32_t getRomCRC32() const;

	enum Platform {
		PLATFORM_UNKNOWN = -1,
		PLATFORM_GB = 1,
		PLATFORM_GBC = 2,
		PLATFORM_SGB = 3,
		PLATFORM_GBA = 4
	};

	std::string m_gameTitle;

	inline static std::atomic<Core*> s_audioRateOwner{nullptr};

	void drainResampledAudio();
	void resetAudioPipeline();
	void applyPendingAudioRateChange();
	void configureCoreAudioBuffer(int sourceRate);
	static void audioRateChangedThunk(struct mAVStream*, unsigned rate);
};
