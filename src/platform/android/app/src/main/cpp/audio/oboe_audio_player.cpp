#include "oboe_audio_player.h"
#include <android/log.h>
#include <algorithm>

#define LOG_TAG "oboe_audio_player"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

bool OboeAudioPlayer::start(int32_t sampleRateHz, size_t requestedRingBufferFrames) {
    if (stream != nullptr) {
        LOGI("start() called but a stream already exists; ignoring");
        return true;
    }

    sampleRate = sampleRateHz;
    ringBufferFrames = requestedRingBufferFrames;
    ringBuffer = std::make_unique<SpscRingBuffer>(ringBufferFrames);

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
        return false;
    }

    LOGI("Oboe stream opened: sampleRate=%d, backend=%s, performanceMode=%s, framesPerBurst=%d",
            stream->getSampleRate(),
            oboe::convertToText(stream->getAudioApi()),
            oboe::convertToText(stream->getPerformanceMode()),
            stream->getFramesPerBurst());

    result = stream->requestStart();
    if (result != oboe::Result::OK) {
        LOGE("Failed to start Oboe stream: %s", oboe::convertToText(result));
        stream->close();
        stream = nullptr;
        return false;
    }

    return true;
}

void OboeAudioPlayer::stop() {
    if (stream == nullptr) return;
    stream->stop();
    stream->close();
    stream = nullptr;
    ringBuffer = nullptr;
}

void OboeAudioPlayer::write(const int16_t* samples, size_t frameCount) {
    if (!ringBuffer) return;
    ringBuffer->write(samples, frameCount);
}

oboe::DataCallbackResult OboeAudioPlayer::onAudioReady(oboe::AudioStream* audioStream, void* audioData, int32_t numFrames) {
    auto* out = static_cast<int16_t*>(audioData);
    if (ringBuffer) {
        ringBuffer->read(out, static_cast<size_t>(numFrames));
        if (isMuted) {
            std::fill(out, out + numFrames * 2, static_cast<int16_t>(0));
        }
    } else {
        std::fill(out, out + numFrames * 2, static_cast<int16_t>(0));
    }
    return oboe::DataCallbackResult::Continue;
}

void OboeAudioPlayer::onErrorAfterClose(oboe::AudioStream* audioStream, oboe::Result error) {
    LOGE("Oboe stream closed after error: %s - attempting to reopen", oboe::convertToText(error));
    int32_t savedSampleRate = sampleRate;
    size_t savedRingBufferFrames = ringBufferFrames;
    stream = nullptr;
    ringBuffer = nullptr;
    if (savedSampleRate > 0 && savedRingBufferFrames > 0) {
        start(savedSampleRate, savedRingBufferFrames);
    }
}