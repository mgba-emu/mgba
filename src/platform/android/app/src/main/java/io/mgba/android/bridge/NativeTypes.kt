package io.mgba.android.bridge

import org.json.JSONObject

data class NativeLoadResult(
    val ok: Boolean,
    val message: String,
    val platform: String,
    val system: String,
    val title: String,
    val displayName: String,
    val crc32: String,
    val gameCode: String,
    val maker: String,
    val version: Int,
    val errorCode: String = "",
) {
    companion object {
        fun fromJson(raw: String): NativeLoadResult {
            val json = JSONObject(raw)
            return NativeLoadResult(
                ok = json.optBoolean("ok", false),
                message = json.optString("message"),
                platform = json.optString("platform"),
                system = json.optString("system"),
                title = json.optString("title"),
                displayName = json.optString("displayName"),
                crc32 = json.optString("crc32"),
                gameCode = json.optString("gameCode"),
                maker = json.optString("maker"),
                version = json.optInt("version", -1),
                errorCode = json.optString("errorCode"),
            )
        }
    }
}

data class NativeStats(
    val frames: Long,
    val videoWidth: Int,
    val videoHeight: Int,
    val videoPixelFormat: String,
    val frameTargetUs: Long,
    val frameActualUs: Long,
    val frameJitterUs: Long,
    val frameLateUs: Long,
    val framePacingSamples: Long,
    val running: Boolean,
    val paused: Boolean,
    val fastForward: Boolean,
    val fastForwardMultiplier: Int,
    val rewinding: Boolean,
    val rewindEnabled: Boolean,
    val rewindBufferCapacity: Int,
    val rewindBufferInterval: Int,
    val scaleMode: Int,
    val filterMode: Int,
    val volumePercent: Int,
    val audioBufferSamples: Int,
    val audioStarted: Boolean,
    val audioPaused: Boolean,
    val audioEnabled: Boolean,
    val audioUnderruns: Long,
    val audioEnqueuedBuffers: Long,
    val audioEnqueuedOutputFrames: Long,
    val audioReadFrames: Long,
    val audioLastReadFrames: Long,
    val audioBackend: String,
    val audioLowPassRange: Int,
    val inputKeys: Int,
    val seenInputKeys: Int,
    val romPlatform: String,
    val gameTitle: String,
    val skipBios: Boolean,
    val gdbStubSupported: Boolean,
    val gdbStubEnabled: Boolean,
    val gdbStubPort: Int,
) {
    companion object {
        private const val InputMask = 0x3FF

        fun fromJson(raw: String): NativeStats {
            val json = runCatching { JSONObject(raw) }.getOrDefault(JSONObject())
            return NativeStats(
                frames = json.optLong("frames", 0L),
                videoWidth = json.optInt("videoWidth", 0),
                videoHeight = json.optInt("videoHeight", 0),
                videoPixelFormat = json.optString("videoPixelFormat", "RGB565"),
                frameTargetUs = json.optLong("frameTargetUs", 0L),
                frameActualUs = json.optLong("frameActualUs", 0L),
                frameJitterUs = json.optLong("frameJitterUs", 0L),
                frameLateUs = json.optLong("frameLateUs", 0L),
                framePacingSamples = json.optLong("framePacingSamples", 0L),
                running = json.optBoolean("running", false),
                paused = json.optBoolean("paused", true),
                fastForward = json.optBoolean("fastForward", false),
                fastForwardMultiplier = json.optInt("fastForwardMultiplier", 0).let {
                    if (it in 2..4) it else 0
                },
                rewinding = json.optBoolean("rewinding", false),
                rewindEnabled = json.optBoolean("rewindEnabled", true),
                rewindBufferCapacity = json.optInt("rewindBufferCapacity", 600),
                rewindBufferInterval = json.optInt("rewindBufferInterval", 1),
                scaleMode = json.optInt("scaleMode", 0),
                filterMode = json.optInt("filterMode", 0),
                volumePercent = json.optInt("volumePercent", 100).coerceIn(0, 100),
                audioBufferSamples = json.optInt("audioBufferSamples", 1024).coerceIn(512, 4096),
                audioStarted = json.optBoolean("audioStarted", false),
                audioPaused = json.optBoolean("audioPaused", true),
                audioEnabled = json.optBoolean("audioEnabled", true),
                audioUnderruns = json.optLong("audioUnderruns", 0L),
                audioEnqueuedBuffers = json.optLong("audioEnqueuedBuffers", 0L),
                audioEnqueuedOutputFrames = json.optLong("audioEnqueuedOutputFrames", 0L),
                audioReadFrames = json.optLong("audioReadFrames", 0L),
                audioLastReadFrames = json.optLong("audioLastReadFrames", 0L),
                audioBackend = json.optString("audioBackend", "OpenSL ES"),
                audioLowPassRange = json.optInt("audioLowPassRange", 0).coerceIn(0, 95),
                inputKeys = json.optInt("inputKeys", 0) and InputMask,
                seenInputKeys = json.optInt("seenInputKeys", 0) and InputMask,
                romPlatform = json.optString("romPlatform"),
                gameTitle = json.optString("gameTitle"),
                skipBios = json.optBoolean("skipBios", false),
                gdbStubSupported = json.optBoolean("gdbStubSupported", false),
                gdbStubEnabled = json.optBoolean("gdbStubEnabled", false),
                gdbStubPort = json.optInt("gdbStubPort", 0).coerceIn(0, 65535),
            )
        }
    }
}

data class NativeGdbStubResult(
    val ok: Boolean,
    val supported: Boolean,
    val enabled: Boolean,
    val port: Int,
    val message: String,
) {
    companion object {
        fun fromJson(raw: String): NativeGdbStubResult {
            val json = runCatching { JSONObject(raw) }.getOrDefault(JSONObject())
            return NativeGdbStubResult(
                ok = json.optBoolean("ok", false),
                supported = json.optBoolean("supported", false),
                enabled = json.optBoolean("enabled", false),
                port = json.optInt("port", 0).coerceIn(0, 65535),
                message = json.optString("message").ifBlank { "GDB stub unavailable" },
            )
        }
    }
}
