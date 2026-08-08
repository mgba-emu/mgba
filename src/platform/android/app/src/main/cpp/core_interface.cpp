#include "core_interface.h"
#include "audio/oboe_audio_player.h"

#include <android/log.h>
#include <cstring>
#include <vector>

extern "C" {
#include "mgba/gb/interface.h"
#include <mgba/core/core.h>
#include <mgba/core/version.h>
#include <mgba/core/serialize.h>
#include <mgba/core/interface.h>
#include <mgba/internal/gb/gb.h>
#include <mgba-util/vfs.h>
}

#define LOG_TAG "core"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

class CoreImpl : public CoreInterface {
public:
    bool init() override {
        LOGI("Core::init");
        return true;
    }

    void shutdown() override {
        unloadRom();
        m_audioPlayer.stop();
    }

    bool quickLoadRom(const uint8_t* data, size_t size) override {
		if (!data || size == 0) {
			LOGE("quickLoadRom: Invalid ROM data.");
			return false;
		}

        if (m_core != nullptr) {
            unloadRom();
        }

		m_romBuffer.assign(data, data + size);

		VFile* vf = VFileFromConstMemory(m_romBuffer.data(), m_romBuffer.size());
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

        if (!m_core->loadROM(m_core, vf)) {
            LOGE("mCore loadROM failed");
            m_core->deinit(m_core);
            m_core = nullptr;
            return false;
        }

        return true;
    }

    bool loadRom(const uint8_t* data, size_t size, bool skipBios, bool rtcEnable) override {
        if (m_core != nullptr) {
            unloadRom();
        }

        m_romBuffer.assign(data, data + size);
        VFile* vf = VFileFromConstMemory(m_romBuffer.data(), m_romBuffer.size());
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

        mCoreConfigSetIntValue(&m_core->config, "skipBios", skipBios ? 1 : 0);
        mCoreConfigSetIntValue(&m_core->config, "hw.rtc", rtcEnable ? 1 : 0);

        LOGI("Config Applied -> Key: '%s' = %d", "skipBios", skipBios ? 1 : 0);
        LOGI("Config Applied -> Key: '%s' = %d", "hw.rtc", rtcEnable ? 1 : 0);

        unsigned width, height;
        m_core->baseVideoSize(m_core, &width, &height);
        m_width = static_cast<int>(width);
        m_height = static_cast<int>(height);

        m_videoBuffer.assign(static_cast<size_t>(m_width) * m_height, 0);

        m_core->setVideoBuffer(m_core, m_videoBuffer.data(), static_cast<size_t>(m_width));

		m_sampleRate = 48000; // best for android
		mCoreConfigSetIntValue(&m_core->config, "sampleRate", m_sampleRate);

        if (!m_core->loadROM(m_core, vf)) {
            LOGE("mCore loadROM failed");
            m_core->deinit(m_core);
            m_core = nullptr;
            return false;
        }

        m_core->setAudioBufferSize(m_core, kAudioPullFrames);

        if (m_audioPlayer.hasStream()) {
            m_audioPlayer.stop();
        }
        if (!m_audioPlayer.start(m_sampleRate, static_cast<size_t>(m_sampleRate) / 10)) {
            LOGE("Failed to start Oboe audio playback, continuing without audio");
        }

        LOGI("ROM loaded: %dx%d, sampleRate=%d", m_width, m_height, m_sampleRate);
		m_core->reset(m_core);
        return true;
    }

    void unloadRom() override {
        if (m_core) {
            m_core->unloadROM(m_core);
            m_core->deinit(m_core);
            m_core = nullptr;
        }
        m_romBuffer.clear();
        m_videoBuffer.clear();
    }

    void reset() override {
        if (m_core) m_core->reset(m_core);
    }

    void runFrame() override {
        if (m_core) m_core->runFrame(m_core);

        int16_t audioBuf[kAudioPullFrames * 2];
        size_t frames = pullAudioSamples(audioBuf, kAudioPullFrames);
        if (frames > 0) {
            m_audioPlayer.write(audioBuf, frames);
        }
    }

    const uint32_t* getVideoBuffer() override {
		return reinterpret_cast<const uint32_t*>(m_videoBuffer.data());
    }

    int getWidth() const override { return m_width; }
    int getHeight() const override { return m_height; }

    void setKeys(uint16_t keyMask) override {
        if (m_core) m_core->setKeys(m_core, static_cast<uint32_t>(keyMask));
    }

    size_t fillAudioBuffer(int16_t* outBuffer, size_t maxFrames) override {
        size_t frames = pullAudioSamples(outBuffer, maxFrames);
        if (frames < maxFrames) {
            std::memset(outBuffer + frames * 2, 0, (maxFrames - frames) * 2 * sizeof(int16_t));
        }
        return maxFrames;
    }

    int getAudioSampleRate() const override {
        return m_sampleRate > 0 ? m_sampleRate : 48000;
    }

    bool saveState(uint8_t* outBuffer, size_t bufferSize, size_t* outWritten) override {
        if (!m_core) { *outWritten = 0; return false; }
        VFile* vf = VFileFromMemory(outBuffer, bufferSize);
        if (!vf) { *outWritten = 0; return false; }

        bool ok = mCoreSaveStateNamed(m_core, vf, SAVESTATE_SAVEDATA | SAVESTATE_SCREENSHOT);
        *outWritten = ok ? static_cast<size_t>(vf->seek(vf, 0, SEEK_CUR)) : 0;
        vf->close(vf);
        return ok;
    }

    bool loadState(const uint8_t* data, size_t size) override {
        if (!m_core) return false;
        VFile* vf = VFileFromConstMemory(data, size);
        if (!vf) return false;
        bool ok = mCoreLoadStateNamed(m_core, vf, SAVESTATE_SAVEDATA | SAVESTATE_SCREENSHOT);
        vf->close(vf);
        return ok;
    }

    bool loadSaveData(const uint8_t* data, size_t size) override {
        if (!m_core) return false;
        bool ok = m_core->savedataRestore(m_core, data, size, true);
        if (!ok) {
            LOGE("savedataRestore failed (size=%zu)", size);
        }
        return ok;
    }

    std::vector<uint8_t> exportSaveData() override {
        if (!m_core) return {};
        void* sram = nullptr;
        size_t size = m_core->savedataClone(m_core, &sram);
        if (size == 0 || sram == nullptr) {
            return {};
        }

        std::vector<uint8_t> result(static_cast<uint8_t*>(sram), static_cast<uint8_t*>(sram) + size);
        free(sram);
        return result;
    }

	std::string getGameTitle() override {
		if (!m_core) return "";
		mGameInfo info{};
		m_core->getGameInfo(m_core, &info);
		info.title[16] = '\0';
		return {info.title};
	}

	std::string getGameCode() override {
		if (!m_core) return "";
		mGameInfo info{};
		m_core->getGameInfo(m_core, &info);
		info.code[4] = '\0';
		return {info.code};
	}

    int getPlatform() override {
        if (!m_core) return PLATFORM_UNKNOWN;

        int basePlatform = m_core->platform(m_core);

        if (basePlatform == mPLATFORM_GBA) {
            return PLATFORM_GBA;
        }

        if (basePlatform == mPLATFORM_GB) {
            struct GB* gbCore = reinterpret_cast<struct GB*>(m_core->board);
            if (!gbCore) return PLATFORM_GB;

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


    bool loadBios(const uint8_t* data, size_t size) override {
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

    void setConfigInt(const char* key, int value) override {
        if (!m_core) return;
        mCoreConfigSetIntValue(&m_core->config, key, value);
    }

    void setConfigString(const char* key, const char* value) override {
        mCoreConfigSetValue(&m_core->config, key, value);
    }

    void setAudioMuted(bool mute) override {
        m_audioPlayer.setMuted(mute);
    }

private:
	size_t pullAudioSamples(int16_t* outBuffer, size_t maxFrames) {
		if (!m_core) return 0;

		struct mAudioBuffer* audioBuffer = m_core->getAudioBuffer(m_core);
		if (!audioBuffer) return 0;

		size_t framesRead = mAudioBufferRead(audioBuffer, outBuffer, maxFrames);
		return framesRead;
	}

    static constexpr size_t kAudioPullFrames = 1024;

    enum Platform {
        PLATFORM_UNKNOWN = -1,
        PLATFORM_GB = 1,
        PLATFORM_GBC = 2,
        PLATFORM_SGB = 3,
        PLATFORM_GBA = 4
    };

    mCore* m_core = nullptr;
    OboeAudioPlayer m_audioPlayer;
    std::vector<uint8_t> m_romBuffer;
    std::vector<uint32_t> m_videoBuffer;
    // only populated and used when color_t is 16-bit; empty and unused otherwise.
    std::vector<uint32_t> m_expandedBuffer;
    int m_width = 0;
    int m_height = 0;
    int m_sampleRate = 0;
};

CoreInterface* createCore() {
    return new CoreImpl();
}
