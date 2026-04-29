#include "AndroidAudioOutput.h"

#include <mgba/core/core.h>

#include <android/api-level.h>

#include <algorithm>
#include <dlfcn.h>
#include <mutex>

namespace mgba::android {

namespace {

constexpr SLuint32 kOpenSlOutputRate = SL_SAMPLINGRATE_48;
constexpr uint32_t kOutputRateHz = 48000;
constexpr size_t kFramesPerBuffer = 800;
constexpr size_t kQueueDepth = 4;
constexpr size_t kWarmupBufferCount = kQueueDepth * 4;
constexpr size_t kChannels = 2;

bool Succeeded(SLresult result) {
	return result == SL_RESULT_SUCCESS;
}

template <typename Function>
bool LoadSymbol(void* library, const char* name, Function* out) {
	*out = reinterpret_cast<Function>(dlsym(library, name));
	return *out != nullptr;
}

} // namespace

struct AndroidAudioOutput::AAudioApi {
	using CreateStreamBuilder = aaudio_result_t (*)(AAudioStreamBuilder**);
	using BuilderDelete = aaudio_result_t (*)(AAudioStreamBuilder*);
	using BuilderSetDirection = void (*)(AAudioStreamBuilder*, aaudio_direction_t);
	using BuilderSetSharingMode = void (*)(AAudioStreamBuilder*, aaudio_sharing_mode_t);
	using BuilderSetPerformanceMode = void (*)(AAudioStreamBuilder*, aaudio_performance_mode_t);
	using BuilderSetSampleRate = void (*)(AAudioStreamBuilder*, int32_t);
	using BuilderSetChannelCount = void (*)(AAudioStreamBuilder*, int32_t);
	using BuilderSetFormat = void (*)(AAudioStreamBuilder*, aaudio_format_t);
	using BuilderOpenStream = aaudio_result_t (*)(AAudioStreamBuilder*, AAudioStream**);
	using StreamRequestStart = aaudio_result_t (*)(AAudioStream*);
	using StreamRequestPause = aaudio_result_t (*)(AAudioStream*);
	using StreamRequestFlush = aaudio_result_t (*)(AAudioStream*);
	using StreamRequestStop = aaudio_result_t (*)(AAudioStream*);
	using StreamClose = aaudio_result_t (*)(AAudioStream*);
	using StreamSetBufferSizeInFrames = int32_t (*)(AAudioStream*, int32_t);
	using StreamGetBufferSizeInFrames = int32_t (*)(AAudioStream*);
	using StreamGetBufferCapacityInFrames = int32_t (*)(AAudioStream*);
	using StreamGetFramesWritten = int64_t (*)(AAudioStream*);
	using StreamGetFramesRead = int64_t (*)(AAudioStream*);
	using StreamWrite = aaudio_result_t (*)(AAudioStream*, const void*, int32_t, int64_t);

	CreateStreamBuilder createStreamBuilder = nullptr;
	BuilderDelete builderDelete = nullptr;
	BuilderSetDirection builderSetDirection = nullptr;
	BuilderSetSharingMode builderSetSharingMode = nullptr;
	BuilderSetPerformanceMode builderSetPerformanceMode = nullptr;
	BuilderSetSampleRate builderSetSampleRate = nullptr;
	BuilderSetChannelCount builderSetChannelCount = nullptr;
	BuilderSetFormat builderSetFormat = nullptr;
	BuilderOpenStream builderOpenStream = nullptr;
	StreamRequestStart streamRequestStart = nullptr;
	StreamRequestPause streamRequestPause = nullptr;
	StreamRequestFlush streamRequestFlush = nullptr;
	StreamRequestStop streamRequestStop = nullptr;
	StreamClose streamClose = nullptr;
	StreamSetBufferSizeInFrames streamSetBufferSizeInFrames = nullptr;
	StreamGetBufferSizeInFrames streamGetBufferSizeInFrames = nullptr;
	StreamGetBufferCapacityInFrames streamGetBufferCapacityInFrames = nullptr;
	StreamGetFramesWritten streamGetFramesWritten = nullptr;
	StreamGetFramesRead streamGetFramesRead = nullptr;
	StreamWrite streamWrite = nullptr;

	static const AAudioApi* Load() {
		if (android_get_device_api_level() < 26) {
			return nullptr;
		}
		static std::once_flag once;
		static AAudioApi api;
		static bool loaded = false;
		std::call_once(once, [] {
			void* library = dlopen("libaaudio.so", RTLD_NOW | RTLD_LOCAL);
			if (!library) {
				return;
			}
			loaded =
			    LoadSymbol(library, "AAudio_createStreamBuilder", &api.createStreamBuilder) &&
			    LoadSymbol(library, "AAudioStreamBuilder_delete", &api.builderDelete) &&
			    LoadSymbol(library, "AAudioStreamBuilder_setDirection", &api.builderSetDirection) &&
			    LoadSymbol(library, "AAudioStreamBuilder_setSharingMode", &api.builderSetSharingMode) &&
			    LoadSymbol(library, "AAudioStreamBuilder_setPerformanceMode", &api.builderSetPerformanceMode) &&
			    LoadSymbol(library, "AAudioStreamBuilder_setSampleRate", &api.builderSetSampleRate) &&
			    LoadSymbol(library, "AAudioStreamBuilder_setChannelCount", &api.builderSetChannelCount) &&
			    LoadSymbol(library, "AAudioStreamBuilder_setFormat", &api.builderSetFormat) &&
			    LoadSymbol(library, "AAudioStreamBuilder_openStream", &api.builderOpenStream) &&
			    LoadSymbol(library, "AAudioStream_requestStart", &api.streamRequestStart) &&
			    LoadSymbol(library, "AAudioStream_requestPause", &api.streamRequestPause) &&
			    LoadSymbol(library, "AAudioStream_requestFlush", &api.streamRequestFlush) &&
			    LoadSymbol(library, "AAudioStream_requestStop", &api.streamRequestStop) &&
			    LoadSymbol(library, "AAudioStream_close", &api.streamClose) &&
			    LoadSymbol(library, "AAudioStream_setBufferSizeInFrames", &api.streamSetBufferSizeInFrames) &&
			    LoadSymbol(library, "AAudioStream_getBufferSizeInFrames", &api.streamGetBufferSizeInFrames) &&
			    LoadSymbol(library, "AAudioStream_getBufferCapacityInFrames", &api.streamGetBufferCapacityInFrames) &&
			    LoadSymbol(library, "AAudioStream_getFramesWritten", &api.streamGetFramesWritten) &&
			    LoadSymbol(library, "AAudioStream_getFramesRead", &api.streamGetFramesRead) &&
			    LoadSymbol(library, "AAudioStream_write", &api.streamWrite);
		});
		return loaded ? &api : nullptr;
	}
};

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
		setOutputPlayingLocked(m_enabled);
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
	m_warmupBuffersRemaining = kWarmupBufferCount;
	setOutputPlayingLocked(m_enabled);
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
	m_outputPlaying = false;
	m_nextBuffer = 0;
	m_warmupBuffersRemaining = 0;
	m_lowPassLeftPrev = 0;
	m_lowPassRightPrev = 0;
	m_enqueuedBuffers = 0;
	m_enqueuedOutputFrames = 0;
	m_readFrames = 0;
	m_lastReadFrames = 0;
}

void AndroidAudioOutput::pause() {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_paused = true;
	setOutputPlayingLocked(false);
	flushOutputLocked();
	if (m_resamplerReady) {
		mAudioBufferClear(&m_resampledBuffer);
	}
	m_warmupBuffersRemaining = kWarmupBufferCount;
}

void AndroidAudioOutput::resume() {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_started) {
		return;
	}
	m_paused = false;
	setOutputPlayingLocked(m_enabled);
}

void AndroidAudioOutput::clear() {
	std::lock_guard<std::mutex> lock(m_mutex);
	flushOutputLocked();
	if (m_resamplerReady) {
		mAudioBufferClear(&m_resampledBuffer);
	}
	m_nextBuffer = 0;
	m_warmupBuffersRemaining = kWarmupBufferCount;
	m_lowPassLeftPrev = 0;
	m_lowPassRightPrev = 0;
}

void AndroidAudioOutput::setEnabled(bool enabled) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_enabled = enabled;
	if (!m_enabled) {
		setOutputPlayingLocked(false);
		flushOutputLocked();
		if (m_resamplerReady) {
			mAudioBufferClear(&m_resampledBuffer);
		}
		m_nextBuffer = 0;
		m_warmupBuffersRemaining = kWarmupBufferCount;
		m_lowPassLeftPrev = 0;
		m_lowPassRightPrev = 0;
	} else if (m_started && !m_paused) {
		setOutputPlayingLocked(true);
	}
}

void AndroidAudioOutput::setVolumePercent(int percent) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_volumePercent = std::clamp(percent, 0, 100);
}

void AndroidAudioOutput::setLowPassRangePercent(int percent) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_lowPassRange = (std::clamp(percent, 0, 95) * 0x10000) / 100;
	m_lowPassLeftPrev = 0;
	m_lowPassRightPrev = 0;
}

uint64_t AndroidAudioOutput::underrunCount() const {
	return m_underrunCount.load();
}

AndroidAudioStats AndroidAudioOutput::stats() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return AndroidAudioStats{
		m_started,
		m_paused,
		m_enabled,
		m_underrunCount.load(),
		m_enqueuedBuffers.load(),
		m_enqueuedOutputFrames.load(),
		m_readFrames.load(),
		m_lastReadFrames.load(),
		backendNameLocked(),
	};
}

void AndroidAudioOutput::resetUnderrunCount() {
	m_underrunCount = 0;
}

void AndroidAudioOutput::enqueueFromCore(mCore* core) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_started || m_paused || !m_enabled || !core) {
		return;
	}

	if (m_backend == AudioBackend::AAudio) {
		if (!m_aaudioApi || !m_aaudioStream) {
			return;
		}
		const int32_t requestedBufferSize = static_cast<int32_t>(kFramesPerBuffer * kQueueDepth);
		const int32_t streamBufferSize = std::max<int32_t>(
		    kFramesPerBuffer,
		    m_aaudioApi->streamGetBufferSizeInFrames(m_aaudioStream));
		const int64_t framesWritten = m_aaudioApi->streamGetFramesWritten(m_aaudioStream);
		const int64_t framesRead = m_aaudioApi->streamGetFramesRead(m_aaudioStream);
		int64_t queuedFrames = std::max<int64_t>(0, framesWritten - framesRead);
		while (queuedFrames + static_cast<int64_t>(kFramesPerBuffer) <= std::max<int32_t>(streamBufferSize, requestedBufferSize)) {
			auto& buffer = m_buffers[m_nextBuffer];
			const size_t readFrames = fillBufferLocked(core, buffer.data(), kFramesPerBuffer);
			const aaudio_result_t written = m_aaudioApi->streamWrite(
			    m_aaudioStream,
			    buffer.data(),
			    static_cast<int32_t>(kFramesPerBuffer),
			    0);
			if (written <= 0) {
				break;
			}
			m_nextBuffer = (m_nextBuffer + 1) % m_buffers.size();
			++m_enqueuedBuffers;
			m_enqueuedOutputFrames += static_cast<uint64_t>(written);
			m_readFrames += readFrames;
			m_lastReadFrames = readFrames;
			queuedFrames += written;
			if (written < static_cast<aaudio_result_t>(kFramesPerBuffer)) {
				break;
			}
		}
		return;
	}

	if (!m_bufferQueue) {
		return;
	}

	SLAndroidSimpleBufferQueueState state = {};
	if (!Succeeded((*m_bufferQueue)->GetState(m_bufferQueue, &state))) {
		return;
	}

	while (state.count < kQueueDepth) {
		auto& buffer = m_buffers[m_nextBuffer];
		const size_t readFrames = fillBufferLocked(core, buffer.data(), kFramesPerBuffer);
		const SLresult result = (*m_bufferQueue)->Enqueue(
			m_bufferQueue,
			buffer.data(),
			static_cast<SLuint32>(buffer.size() * sizeof(int16_t)));
		if (!Succeeded(result)) {
			break;
		}
		m_nextBuffer = (m_nextBuffer + 1) % m_buffers.size();
		++m_enqueuedBuffers;
		m_enqueuedOutputFrames += kFramesPerBuffer;
		m_readFrames += readFrames;
		m_lastReadFrames = readFrames;
		++state.count;
	}
}

void AndroidAudioOutput::BufferQueueCallback(SLAndroidSimpleBufferQueueItf, void*) {
}

bool AndroidAudioOutput::createEngineLocked() {
	if (createAAudioStreamLocked()) {
		m_backend = AudioBackend::AAudio;
		return true;
	}
	m_backend = AudioBackend::OpenSl;
	return createOpenSlEngineLocked();
}

void AndroidAudioOutput::destroyEngineLocked() {
	destroyAAudioStreamLocked();
	destroyOpenSlEngineLocked();
	m_backend = AudioBackend::OpenSl;
}

bool AndroidAudioOutput::createAAudioStreamLocked() {
	m_aaudioApi = AAudioApi::Load();
	if (!m_aaudioApi) {
		return false;
	}

	AAudioStreamBuilder* builder = nullptr;
	if (m_aaudioApi->createStreamBuilder(&builder) != AAUDIO_OK || !builder) {
		return false;
	}
	m_aaudioApi->builderSetDirection(builder, AAUDIO_DIRECTION_OUTPUT);
	m_aaudioApi->builderSetSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
	m_aaudioApi->builderSetPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
	m_aaudioApi->builderSetSampleRate(builder, static_cast<int32_t>(kOutputRateHz));
	m_aaudioApi->builderSetChannelCount(builder, static_cast<int32_t>(kChannels));
	m_aaudioApi->builderSetFormat(builder, AAUDIO_FORMAT_PCM_I16);

	AAudioStream* stream = nullptr;
	const aaudio_result_t result = m_aaudioApi->builderOpenStream(builder, &stream);
	m_aaudioApi->builderDelete(builder);
	if (result != AAUDIO_OK || !stream) {
		return false;
	}

	m_aaudioStream = stream;
	m_aaudioApi->streamSetBufferSizeInFrames(m_aaudioStream, static_cast<int32_t>(kFramesPerBuffer * kQueueDepth));
	return true;
}

void AndroidAudioOutput::destroyAAudioStreamLocked() {
	if (m_aaudioStream && m_aaudioApi) {
		m_aaudioApi->streamRequestStop(m_aaudioStream);
		m_aaudioApi->streamClose(m_aaudioStream);
	}
	m_aaudioStream = nullptr;
	m_outputPlaying = false;
}

bool AndroidAudioOutput::createOpenSlEngineLocked() {
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
		kOpenSlOutputRate,
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

void AndroidAudioOutput::destroyOpenSlEngineLocked() {
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
	m_outputPlaying = false;
}

void AndroidAudioOutput::setOutputPlayingLocked(bool playing) {
	if (m_outputPlaying == playing) {
		return;
	}
	if (m_backend == AudioBackend::AAudio) {
		if (!m_aaudioStream || !m_aaudioApi) {
			return;
		}
		if (playing) {
			m_aaudioApi->streamRequestStart(m_aaudioStream);
		} else {
			m_aaudioApi->streamRequestPause(m_aaudioStream);
		}
		m_outputPlaying = playing;
		return;
	}
	if (m_player) {
		(*m_player)->SetPlayState(m_player, playing ? SL_PLAYSTATE_PLAYING : SL_PLAYSTATE_PAUSED);
		m_outputPlaying = playing;
	}
}

void AndroidAudioOutput::flushOutputLocked() {
	if (m_backend == AudioBackend::AAudio) {
		if (!m_aaudioStream || !m_aaudioApi) {
			return;
		}
		if (m_outputPlaying) {
			m_aaudioApi->streamRequestPause(m_aaudioStream);
		}
		m_outputPlaying = false;
		m_aaudioApi->streamRequestFlush(m_aaudioStream);
		if (m_started && !m_paused && m_enabled) {
			m_aaudioApi->streamRequestStart(m_aaudioStream);
			m_outputPlaying = true;
		}
		return;
	}
	if (m_bufferQueue) {
		(*m_bufferQueue)->Clear(m_bufferQueue);
	}
}

const char* AndroidAudioOutput::backendNameLocked() const {
	return m_backend == AudioBackend::AAudio ? "AAudio" : "OpenSL ES";
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
	const size_t readFrames = mAudioBufferRead(&m_resampledBuffer, output, frames);
	if (readFrames < frames) {
		if (m_warmupBuffersRemaining > 0) {
			--m_warmupBuffersRemaining;
		} else {
			++m_underrunCount;
		}
	} else if (m_warmupBuffersRemaining > 0) {
		--m_warmupBuffersRemaining;
	}
	const int lowPassRange = m_lowPassRange;
	if (lowPassRange > 0) {
		int32_t left = m_lowPassLeftPrev;
		int32_t right = m_lowPassRightPrev;
		const int32_t factorB = 0x10000 - lowPassRange;
		for (size_t frame = 0; frame < readFrames; ++frame) {
			int16_t* sample = output + frame * kChannels;
			left = (left * lowPassRange) + (static_cast<int32_t>(sample[0]) * factorB);
			right = (right * lowPassRange) + (static_cast<int32_t>(sample[1]) * factorB);
			left >>= 16;
			right >>= 16;
			sample[0] = static_cast<int16_t>(left);
			sample[1] = static_cast<int16_t>(right);
		}
		m_lowPassLeftPrev = left;
		m_lowPassRightPrev = right;
	}
	const int volumePercent = m_volumePercent;
	if (volumePercent < 100) {
		const size_t samples = readFrames * kChannels;
		for (size_t i = 0; i < samples; ++i) {
			output[i] = static_cast<int16_t>((static_cast<int32_t>(output[i]) * volumePercent) / 100);
		}
	}
	return readFrames;
}

} // namespace mgba::android
