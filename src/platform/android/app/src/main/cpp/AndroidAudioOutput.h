#ifndef MGBA_ANDROID_AUDIO_OUTPUT_H
#define MGBA_ANDROID_AUDIO_OUTPUT_H

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <aaudio/AAudio.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <mgba-util/audio-buffer.h>
#include <mgba-util/audio-resampler.h>
#include <mutex>
#include <vector>

struct mCore;

namespace mgba::android {

struct AndroidAudioStats {
	bool started = false;
	bool paused = true;
	bool enabled = true;
	uint64_t underruns = 0;
	uint64_t enqueuedBuffers = 0;
	uint64_t enqueuedOutputFrames = 0;
	uint64_t readFrames = 0;
	uint64_t lastReadFrames = 0;
	const char* backend = "OpenSL ES";
};

class AndroidAudioOutput {
public:
	AndroidAudioOutput();
	~AndroidAudioOutput();

	bool start();
	void stop();
	void pause();
	void resume();
	void clear();
	void setEnabled(bool enabled);
	void setVolumePercent(int percent);
	void setLowPassRangePercent(int percent);
	uint64_t underrunCount() const;
	AndroidAudioStats stats() const;
	void resetUnderrunCount();
	void enqueueFromCore(mCore* core, int speedPercent);

private:
	struct AAudioApi;
	enum class AudioBackend {
		OpenSl,
		AAudio,
	};

	static void BufferQueueCallback(SLAndroidSimpleBufferQueueItf queue, void* context);

	bool createEngineLocked();
	void destroyEngineLocked();
	bool createAAudioStreamLocked();
	void destroyAAudioStreamLocked();
	bool createOpenSlEngineLocked();
	void destroyOpenSlEngineLocked();
	void setOutputPlayingLocked(bool playing);
	void flushOutputLocked();
	const char* backendNameLocked() const;
	size_t fillBufferLocked(mCore* core, int16_t* output, size_t frames, int speedPercent);

	mutable std::mutex m_mutex;
	AudioBackend m_backend = AudioBackend::OpenSl;
	bool m_started = false;
	bool m_paused = true;
	bool m_enabled = true;
	bool m_outputPlaying = false;
	bool m_resamplerReady = false;
	int m_volumePercent = 100;
	int m_lowPassRange = 0;
	int32_t m_lowPassLeftPrev = 0;
	int32_t m_lowPassRightPrev = 0;

	SLObjectItf m_engineObject = nullptr;
	SLEngineItf m_engine = nullptr;
	SLObjectItf m_outputMixObject = nullptr;
	SLObjectItf m_playerObject = nullptr;
	SLPlayItf m_player = nullptr;
	SLAndroidSimpleBufferQueueItf m_bufferQueue = nullptr;

	const AAudioApi* m_aaudioApi = nullptr;
	AAudioStream* m_aaudioStream = nullptr;

	struct mAudioBuffer m_resampledBuffer = {};
	struct mAudioResampler m_resampler = {};
	std::array<std::vector<int16_t>, 4> m_buffers;
	size_t m_nextBuffer = 0;
	size_t m_warmupBuffersRemaining = 0;
	std::atomic<uint64_t> m_underrunCount{0};
	std::atomic<uint64_t> m_enqueuedBuffers{0};
	std::atomic<uint64_t> m_enqueuedOutputFrames{0};
	std::atomic<uint64_t> m_readFrames{0};
	std::atomic<uint64_t> m_lastReadFrames{0};
};

} // namespace mgba::android

#endif
