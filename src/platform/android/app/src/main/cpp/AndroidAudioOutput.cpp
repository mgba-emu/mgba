#include "AndroidAudioOutput.h"

#include <mgba/core/core.h>

#include <algorithm>

namespace mgba::android {

namespace {

constexpr SLuint32 kOutputRate = SL_SAMPLINGRATE_48;
constexpr uint32_t kOutputRateHz = 48000;
constexpr size_t kFramesPerBuffer = 1024;
constexpr size_t kQueueDepth = 4;
constexpr size_t kChannels = 2;

bool Succeeded(SLresult result) {
	return result == SL_RESULT_SUCCESS;
}

} // namespace

AndroidAudioOutput::AndroidAudioOutput() {
	for (auto& buffer : m_buffers) {
		buffer.resize(kFramesPerBuffer * kChannels);
	}
}

AndroidAudioOutput::~AndroidAudioOutput() {
	stop();
}

bool AndroidAudioOutput::start() {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_started) {
		m_paused = false;
		if (m_player) {
			(*m_player)->SetPlayState(m_player, SL_PLAYSTATE_PLAYING);
		}
		return true;
	}

	mAudioBufferInit(&m_resampledBuffer, kFramesPerBuffer * kQueueDepth * 4, kChannels);
	mAudioResamplerInit(&m_resampler, mINTERPOLATOR_COSINE);
	mAudioResamplerSetDestination(&m_resampler, &m_resampledBuffer, kOutputRateHz);
	m_resamplerReady = true;

	if (!createEngineLocked()) {
		if (m_resamplerReady) {
			mAudioResamplerDeinit(&m_resampler);
			mAudioBufferDeinit(&m_resampledBuffer);
			m_resamplerReady = false;
		}
		return false;
	}

	m_started = true;
	m_paused = false;
	if (m_player) {
		(*m_player)->SetPlayState(m_player, SL_PLAYSTATE_PLAYING);
	}
	return true;
}

void AndroidAudioOutput::stop() {
	std::lock_guard<std::mutex> lock(m_mutex);
	destroyEngineLocked();
	if (m_resamplerReady) {
		mAudioResamplerDeinit(&m_resampler);
		mAudioBufferDeinit(&m_resampledBuffer);
		m_resamplerReady = false;
	}
	m_started = false;
	m_paused = true;
	m_nextBuffer = 0;
}

void AndroidAudioOutput::pause() {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_paused = true;
	if (m_player) {
		(*m_player)->SetPlayState(m_player, SL_PLAYSTATE_PAUSED);
	}
	if (m_bufferQueue) {
		(*m_bufferQueue)->Clear(m_bufferQueue);
	}
	if (m_resamplerReady) {
		mAudioBufferClear(&m_resampledBuffer);
	}
}

void AndroidAudioOutput::resume() {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_started) {
		return;
	}
	m_paused = false;
	if (m_player) {
		(*m_player)->SetPlayState(m_player, SL_PLAYSTATE_PLAYING);
	}
}

void AndroidAudioOutput::clear() {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_bufferQueue) {
		(*m_bufferQueue)->Clear(m_bufferQueue);
	}
	if (m_resamplerReady) {
		mAudioBufferClear(&m_resampledBuffer);
	}
	m_nextBuffer = 0;
}

void AndroidAudioOutput::enqueueFromCore(mCore* core) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_started || m_paused || !m_bufferQueue || !core) {
		return;
	}

	SLAndroidSimpleBufferQueueState state = {};
	if (!Succeeded((*m_bufferQueue)->GetState(m_bufferQueue, &state))) {
		return;
	}

	while (state.count < kQueueDepth) {
		auto& buffer = m_buffers[m_nextBuffer];
		fillBufferLocked(core, buffer.data(), kFramesPerBuffer);
		const SLresult result = (*m_bufferQueue)->Enqueue(
			m_bufferQueue,
			buffer.data(),
			static_cast<SLuint32>(buffer.size() * sizeof(int16_t)));
		if (!Succeeded(result)) {
			break;
		}
		m_nextBuffer = (m_nextBuffer + 1) % m_buffers.size();
		++state.count;
	}
}

void AndroidAudioOutput::BufferQueueCallback(SLAndroidSimpleBufferQueueItf, void*) {
}

bool AndroidAudioOutput::createEngineLocked() {
	if (!Succeeded(slCreateEngine(&m_engineObject, 0, nullptr, 0, nullptr, nullptr))) {
		destroyEngineLocked();
		return false;
	}
	if (!Succeeded((*m_engineObject)->Realize(m_engineObject, SL_BOOLEAN_FALSE))) {
		destroyEngineLocked();
		return false;
	}
	if (!Succeeded((*m_engineObject)->GetInterface(m_engineObject, SL_IID_ENGINE, &m_engine))) {
		destroyEngineLocked();
		return false;
	}
	if (!Succeeded((*m_engine)->CreateOutputMix(m_engine, &m_outputMixObject, 0, nullptr, nullptr))) {
		destroyEngineLocked();
		return false;
	}
	if (!Succeeded((*m_outputMixObject)->Realize(m_outputMixObject, SL_BOOLEAN_FALSE))) {
		destroyEngineLocked();
		return false;
	}

	SLDataLocator_AndroidSimpleBufferQueue locatorBufferQueue = {
		SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
		static_cast<SLuint32>(kQueueDepth),
	};
	SLDataFormat_PCM formatPcm = {
		SL_DATAFORMAT_PCM,
		static_cast<SLuint32>(kChannels),
		kOutputRate,
		SL_PCMSAMPLEFORMAT_FIXED_16,
		SL_PCMSAMPLEFORMAT_FIXED_16,
		SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
		SL_BYTEORDER_LITTLEENDIAN,
	};
	SLDataSource audioSource = {
		&locatorBufferQueue,
		&formatPcm,
	};
	SLDataLocator_OutputMix locatorOutputMix = {
		SL_DATALOCATOR_OUTPUTMIX,
		m_outputMixObject,
	};
	SLDataSink audioSink = {
		&locatorOutputMix,
		nullptr,
	};
	const SLInterfaceID interfaces[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
	const SLboolean required[] = {SL_BOOLEAN_TRUE};

	if (!Succeeded((*m_engine)->CreateAudioPlayer(
		    m_engine,
		    &m_playerObject,
		    &audioSource,
		    &audioSink,
		    1,
		    interfaces,
		    required))) {
		destroyEngineLocked();
		return false;
	}
	if (!Succeeded((*m_playerObject)->Realize(m_playerObject, SL_BOOLEAN_FALSE))) {
		destroyEngineLocked();
		return false;
	}
	if (!Succeeded((*m_playerObject)->GetInterface(m_playerObject, SL_IID_PLAY, &m_player))) {
		destroyEngineLocked();
		return false;
	}
	if (!Succeeded((*m_playerObject)->GetInterface(m_playerObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &m_bufferQueue))) {
		destroyEngineLocked();
		return false;
	}
	if (!Succeeded((*m_bufferQueue)->RegisterCallback(m_bufferQueue, BufferQueueCallback, this))) {
		destroyEngineLocked();
		return false;
	}

	return true;
}

void AndroidAudioOutput::destroyEngineLocked() {
	if (m_player) {
		(*m_player)->SetPlayState(m_player, SL_PLAYSTATE_STOPPED);
	}
	if (m_bufferQueue) {
		(*m_bufferQueue)->Clear(m_bufferQueue);
	}
	if (m_playerObject) {
		(*m_playerObject)->Destroy(m_playerObject);
	}
	if (m_outputMixObject) {
		(*m_outputMixObject)->Destroy(m_outputMixObject);
	}
	if (m_engineObject) {
		(*m_engineObject)->Destroy(m_engineObject);
	}
	m_playerObject = nullptr;
	m_player = nullptr;
	m_bufferQueue = nullptr;
	m_outputMixObject = nullptr;
	m_engineObject = nullptr;
	m_engine = nullptr;
}

size_t AndroidAudioOutput::fillBufferLocked(mCore* core, int16_t* output, size_t frames) {
	std::fill(output, output + frames * kChannels, 0);
	if (!m_resamplerReady || !core || !core->getAudioBuffer || !core->audioSampleRate) {
		return 0;
	}

	struct mAudioBuffer* source = core->getAudioBuffer(core);
	const unsigned sampleRate = core->audioSampleRate(core);
	if (!source || !sampleRate) {
		return 0;
	}

	mAudioResamplerSetSource(&m_resampler, source, sampleRate, true);
	mAudioResamplerProcess(&m_resampler);
	return mAudioBufferRead(&m_resampledBuffer, output, frames);
}

} // namespace mgba::android
