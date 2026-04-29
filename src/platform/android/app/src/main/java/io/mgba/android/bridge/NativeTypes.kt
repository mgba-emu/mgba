package io.mgba.android.bridge

import org.json.JSONObject

data class NativeLoadResult(
    val ok: Boolean,
    val message: String,
    val platform: String,
    val title: String,
    val displayName: String,
    val crc32: String,
) {
    companion object {
        fun fromJson(raw: String): NativeLoadResult {
            val json = JSONObject(raw)
            return NativeLoadResult(
                ok = json.optBoolean("ok", false),
                message = json.optString("message"),
                platform = json.optString("platform"),
                title = json.optString("title"),
                displayName = json.optString("displayName"),
                crc32 = json.optString("crc32"),
            )
        }
    }
}

data class NativeStats(
    val frames: Long,
    val videoWidth: Int,
    val videoHeight: Int,
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
    val audioUnderruns: Long,
    val audioLowPassRange: Int,
    val romPlatform: String,
    val gameTitle: String,
    val skipBios: Boolean,
) {
    companion object {
        fun fromJson(raw: String): NativeStats {
            val json = runCatching { JSONObject(raw) }.getOrDefault(JSONObject())
            return NativeStats(
                frames = json.optLong("frames", 0L),
                videoWidth = json.optInt("videoWidth", 0),
                videoHeight = json.optInt("videoHeight", 0),
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
                audioUnderruns = json.optLong("audioUnderruns", 0L),
                audioLowPassRange = json.optInt("audioLowPassRange", 0).coerceIn(0, 95),
                romPlatform = json.optString("romPlatform"),
                gameTitle = json.optString("gameTitle"),
                skipBios = json.optBoolean("skipBios", false),
            )
        }
    }
}
