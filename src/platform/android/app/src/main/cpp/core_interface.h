#pragma once
#include <cstdint>
#include <cstddef>
#include <string>

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

class CoreInterface {
public:
    virtual ~CoreInterface() = default;

    virtual bool init() = 0;
    virtual void shutdown() = 0;

    virtual bool loadRom(const uint8_t* data, size_t size, bool skipBios, bool rtcEnable) = 0;
    virtual bool quickLoadRom(const uint8_t* data, size_t size) = 0;
    virtual void unloadRom() = 0;
    virtual void reset() = 0;

    virtual void runFrame() = 0;

    virtual const uint32_t* getVideoBuffer() = 0;
    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;

    virtual void setKeys(uint16_t keyMask) = 0;

    virtual size_t fillAudioBuffer(int16_t* outBuffer, size_t maxFrames) = 0;
    virtual int getAudioSampleRate() const = 0;

    virtual bool saveState(uint8_t* outBuffer, size_t bufferSize, size_t* outWritten) = 0;
    virtual bool loadState(const uint8_t* data, size_t size) = 0;

    virtual bool loadSaveData(const uint8_t* data, size_t size) = 0;
    virtual std::vector<uint8_t> exportSaveData() = 0;

    virtual std::string getGameTitle() = 0;
    virtual std::string getGameCode() = 0;

    virtual int getPlatform() = 0;

    virtual bool loadBios(const uint8_t* data, size_t size) = 0;

    virtual void setConfigInt(const char* key, int value) = 0;
    virtual void setConfigString(const char* key, const char* value) = 0;

    virtual void setAudioMuted(bool mute) = 0;
};

CoreInterface* createCore();
