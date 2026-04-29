#ifndef MGBA_ANDROID_AUDIO_OUTPUT_H
#define MGBA_ANDROID_AUDIO_OUTPUT_H

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <mgba-util/audio-buffer.h>
#include <mgba-util/audio-resampler.h>
#include <mutex>
#include <vector>

struct mCore;

namespace mgba::android {

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
	void resetUnderrunCount();
	void enqueueFromCore(mCore* core);

private:
	static void BufferQueueCallback(SLAndroidSimpleBufferQueueItf queue, void* context);

	bool createEngineLocked();
	void destroyEngineLocked();
	size_t fillBufferLocked(mCore* core, int16_t* output, size_t frames);

	std::mutex m_mutex;
	bool m_started = false;
	bool m_paused = true;
	bool m_enabled = true;
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

	struct mAudioBuffer m_resampledBuffer = {};
	struct mAudioResampler m_resampler = {};
	std::array<std::vector<int16_t>, 4> m_buffers;
	size_t m_nextBuffer = 0;
	std::atomic<uint64_t> m_underrunCount{0};
};

} // namespace mgba::android

#endif
