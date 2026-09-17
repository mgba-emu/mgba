/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

#include "../log/log.h"
#include "oboe_audio_player.h"
#include <algorithm>
#include <chrono>

extern "C" {
#include <mgba-util/math.h>
}

class SpscRingBuffer {
public:
	explicit SpscRingBuffer(size_t capacityFrames)
	    : capacity(toPow2(capacityFrames)),
	    mask(capacity - 1),
	    buffer(capacity * kChannels) {}

	size_t write(const int16_t* src, size_t frameCount) {
		size_t w = writeIndex.load(std::memory_order_relaxed);
		size_t r = readIndex.load(std::memory_order_acquire);
		size_t freeFrames = capacity - (w - r);
		size_t toWrite = std::min(frameCount, freeFrames);

		for (size_t i = 0; i < toWrite; ++i) {
			size_t idx = (w + i) & mask;
			buffer[idx * kChannels] = src[i * kChannels];
			buffer[idx * kChannels + 1] = src[i * kChannels + 1];
		}

		if (toWrite < frameCount) {
			overflow.fetch_add(frameCount - toWrite, std::memory_order_relaxed);
		}

		writeIndex.store(w + toWrite, std::memory_order_release);
		return toWrite;
	}

	size_t available() const {
		const size_t r = readIndex.load(std::memory_order_acquire);
		const size_t w = writeIndex.load(std::memory_order_acquire);
		return w - r;
	}

	size_t capacityFrames() const { return capacity; }

	size_t freeFrames() const {
		const size_t r = readIndex.load(std::memory_order_acquire);
		const size_t w = writeIndex.load(std::memory_order_acquire);
		return capacity - (w - r);
	}

	uint64_t overflowFrames() const { return overflow.load(std::memory_order_relaxed); }
	uint64_t underrunFrames() const { return underrun.load(std::memory_order_relaxed); }

	size_t read(int16_t* dst, size_t frameCount) {
		size_t r = readIndex.load(std::memory_order_relaxed);
		size_t w = writeIndex.load(std::memory_order_acquire);
		size_t availableFrames = w - r;
		size_t toRead = std::min(frameCount, availableFrames);

		for (size_t i = 0; i < toRead; ++i) {
			size_t idx = (r + i) & mask;
			dst[i * kChannels] = buffer[idx * kChannels];
			dst[i * kChannels + 1] = buffer[idx * kChannels + 1];
		}

		if (toRead < frameCount) {
			underrun.fetch_add(frameCount - toRead, std::memory_order_relaxed);
		}

		for (size_t i = toRead; i < frameCount; ++i) {
			dst[i * kChannels] = 0;
			dst[i * kChannels + 1] = 0;
		}

		readIndex.store(r + toRead, std::memory_order_release);
		return toRead;
	}

private:
	static constexpr size_t kChannels = 2;

	const size_t capacity;
	const size_t mask;
	std::vector<int16_t> buffer;
	std::atomic<size_t> writeIndex{0};
	std::atomic<size_t> readIndex{0};
	std::atomic<uint64_t> overflow{0};
	std::atomic<uint64_t> underrun{0};
};

OboeAudioPlayer::OboeAudioPlayer() = default;

OboeAudioPlayer::~OboeAudioPlayer() {
	stop();
}

bool OboeAudioPlayer::start(int32_t sampleRateHz, size_t requestedRingBufferFrames) {
    if (stream != nullptr) {
        LOGI("start() called but a stream already exists; ignoring");
        return true;
    }

    sampleRate = sampleRateHz;
    framesPerBurst = 0;
    totalFramesWritten.store(0, std::memory_order_relaxed);
    totalFramesConsumed.store(0, std::memory_order_relaxed);
    ringBufferFrames = requestedRingBufferFrames;
    ringBuffer = std::make_unique<SpscRingBuffer>(ringBufferFrames);
    playbackStarted.store(false, std::memory_order_release);

    oboe::AudioStreamBuilder builder;
    oboe::Result result = builder.setDirection(oboe::Direction::Output)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setSharingMode(oboe::SharingMode::Exclusive)
            ->setUsage(oboe::Usage::Game)
            ->setSampleRate(sampleRate)
            ->setChannelCount(oboe::ChannelCount::Stereo)
            ->setFormat(oboe::AudioFormat::I16)
            ->setDataCallback(this)
            ->setErrorCallback(this)
            ->openStream(stream);

    if (result != oboe::Result::OK) {
        LOGE("Failed to open Oboe stream: %s", oboe::convertToText(result));
        stream = nullptr;
        ringBuffer = nullptr;
        return false;
    }

    sampleRate = stream->getSampleRate();
    framesPerBurst = stream->getFramesPerBurst();

    LOGI("Oboe stream opened: sampleRate=%d, backend=%s, performanceMode=%s, sharingMode=%s, framesPerBurst=%d",
         sampleRate,
         oboe::convertToText(stream->getAudioApi()),
         oboe::convertToText(stream->getPerformanceMode()),
         oboe::convertToText(stream->getSharingMode()),
         framesPerBurst);

    LOGI("Oboe stream opened; waiting for prebuffer=%zu frames (ring capacity=%zu)",
         kPrebufferFrames, ringBuffer->capacityFrames());

    return true;
}

void OboeAudioPlayer::stop() {
    playbackStarted.store(false, std::memory_order_release);
    if (stream == nullptr) {
        ringBuffer = nullptr;
        return;
    }

    stream->stop();
    stream->close();
    stream = nullptr;
    ringBuffer = nullptr;
    framesPerBurst = 0;
}

size_t OboeAudioPlayer::write(const int16_t* samples, size_t frameCount) {
    if (!ringBuffer || !samples || frameCount == 0) return 0;

    const size_t written = ringBuffer->write(samples, frameCount);
    totalFramesWritten.fetch_add(written, std::memory_order_relaxed);

    if (written < frameCount) {
        LOGD("audio ring short write: read=%zu written=%zu queued=%zu/%zu",
             frameCount,
             written,
             ringBuffer->available(),
             ringBuffer->capacityFrames()
		);
        return written;
    }

    if (stream && !playbackStarted.load(std::memory_order_acquire) &&
        ringBuffer->available() >= kPrebufferFrames) {
        bool expected = false;
        if (playbackStarted.compare_exchange_strong(
                expected, true,
                std::memory_order_acq_rel,
                std::memory_order_acquire)
		) {
            const oboe::Result result = stream->requestStart();
            if (result == oboe::Result::OK) {
                LOGI("Starting Oboe playback after prebuffer: queued=%zu/%zu",
				     ringBuffer->available(), ringBuffer->capacityFrames());
            } else {
                playbackStarted.store(false, std::memory_order_release);
                LOGE("Failed to start Oboe stream after prebuffer: %s", oboe::convertToText(result));
            }
        }
    }

    return written;
}

size_t OboeAudioPlayer::availableFrames() const {
    return ringBuffer ? ringBuffer->available() : 0;
}

size_t OboeAudioPlayer::freeFrames() const {
    return ringBuffer ? ringBuffer->freeFrames() : 0;
}

size_t OboeAudioPlayer::capacityFrames() const {
    return ringBuffer ? ringBuffer->capacityFrames() : 0;
}

uint64_t OboeAudioPlayer::getTotalFramesWritten() const {
    return totalFramesWritten.load(std::memory_order_relaxed);
}

oboe::DataCallbackResult OboeAudioPlayer::onAudioReady(oboe::AudioStream*, void* audioData, int32_t numFrames) {
    auto* out = static_cast<int16_t*>(audioData);

    if (ringBuffer) {
        ringBuffer->read(out, static_cast<size_t>(numFrames));
        totalFramesConsumed.fetch_add(static_cast<uint64_t>(numFrames), std::memory_order_relaxed);
        if (isMuted.load(std::memory_order_relaxed)) {
            std::fill(out, out + numFrames * 2, static_cast<int16_t>(0));
        }
    } else {
        std::fill(out, out + numFrames * 2, static_cast<int16_t>(0));
    }

    static thread_local auto lastStatsTime = std::chrono::steady_clock::now();
    static thread_local uint64_t lastWrittenFrames = 0;
    static thread_local uint64_t lastConsumedFrames = 0;

    const auto now = std::chrono::steady_clock::now();
    if (now - lastStatsTime >= std::chrono::seconds(1)) {
        const uint64_t totalWritten = getTotalFramesWritten();
        const uint64_t totalConsumed = getTotalFramesConsumed();
        const uint64_t writtenSinceLast = totalWritten - lastWrittenFrames;
        const uint64_t consumedSinceLast = totalConsumed - lastConsumedFrames;

        LOGD("audio stats: written=%llu/s consumed=%llu/s total=%llu queued=%zu/%zu overflow=%llu underrun=%llu xrun=%d rate=%d burst=%d",
             static_cast<unsigned long long>(writtenSinceLast),
             static_cast<unsigned long long>(consumedSinceLast),
             static_cast<unsigned long long>(totalWritten),
             availableFrames(),
             capacityFrames(),
             static_cast<unsigned long long>(getOverflowFrames()),
             static_cast<unsigned long long>(getUnderrunFrames()),
             getStreamXRunCount(),
             getSampleRate(),
             getFramesPerBurst()
		);

        lastWrittenFrames = totalWritten;
        lastConsumedFrames = totalConsumed;
        lastStatsTime = now;
    }

    return oboe::DataCallbackResult::Continue;
}

void OboeAudioPlayer::onErrorAfterClose(oboe::AudioStream*, oboe::Result error) {
    LOGE("Oboe stream closed after error: %s - attempting to reopen", oboe::convertToText(error));

    const int32_t savedSampleRate = sampleRate;
    const size_t savedRingBufferFrames = ringBufferFrames;

    stream = nullptr;
    ringBuffer = nullptr;
    playbackStarted.store(false, std::memory_order_release);

    if (savedSampleRate > 0 && savedRingBufferFrames > 0) {
        start(savedSampleRate, savedRingBufferFrames);
    }
}

int32_t OboeAudioPlayer::getStreamXRunCount() const {
    if (!stream) return 0;
    auto result = stream->getXRunCount();
    return result ? result.value() : 0;
}

uint64_t OboeAudioPlayer::getUnderrunFrames() const {
	return ringBuffer ? ringBuffer->underrunFrames() : 0;
}

uint64_t OboeAudioPlayer::getOverflowFrames() const {
	return ringBuffer ? ringBuffer->overflowFrames() : 0;
}
