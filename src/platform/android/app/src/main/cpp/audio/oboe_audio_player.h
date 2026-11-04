/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

#pragma once

#include <oboe/Oboe.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <vector>

class SpscRingBuffer;

class OboeAudioPlayer : public oboe::AudioStreamDataCallback, public oboe::AudioStreamErrorCallback {
public:
	OboeAudioPlayer();
	~OboeAudioPlayer();

    bool start(int32_t sampleRateHz, size_t ringBufferFrames);
    void stop();

    void setMuted(bool mute) { isMuted = mute; }

    size_t write(const int16_t* samples, size_t frameCount);

    size_t availableFrames() const;
    size_t freeFrames() const;
    size_t capacityFrames() const;

    uint64_t getTotalFramesWritten() const;
    int32_t getStreamXRunCount() const;

    int32_t getSampleRate() const { return sampleRate; }
    int32_t getFramesPerBurst() const { return framesPerBurst; }

    uint64_t getTotalFramesConsumed() const {
		return totalFramesConsumed.load(std::memory_order_relaxed);
	}

    uint64_t getUnderrunFrames() const;
    uint64_t getOverflowFrames() const;

    bool hasStream() const { return stream != nullptr; }

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream* stream, void* audioData, int32_t numFrames) override;
    void onErrorAfterClose(oboe::AudioStream* stream, oboe::Result error) override;

private:
    std::shared_ptr<oboe::AudioStream> stream;
    std::unique_ptr<SpscRingBuffer> ringBuffer;
    int32_t sampleRate = 0;
    size_t ringBufferFrames = 0;
    std::atomic<bool> isMuted{false};
    std::atomic<bool> playbackStarted{false};
    std::atomic<uint64_t> totalFramesWritten{0};
    std::atomic<uint64_t> totalFramesConsumed{0};
    int32_t framesPerBurst = 0;
    static constexpr size_t kPrebufferFrames = 4096;
};