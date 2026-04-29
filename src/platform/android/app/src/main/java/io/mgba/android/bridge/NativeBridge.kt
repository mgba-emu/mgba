package io.mgba.android.bridge

import android.view.Surface
import org.json.JSONArray
import org.json.JSONObject

data class NativeArchiveEntry(
    val name: String,
    val displayName: String,
)

object NativeBridge {
    init {
        System.loadLibrary("mgba-android")
    }

    @JvmStatic
    external fun nativeGetVersion(): String

    @JvmStatic
    external fun nativeCreate(basePath: String, cachePath: String): Long

    @JvmStatic
    external fun nativeDestroy(handle: Long)

    @JvmStatic
    external fun nativeLoadRomFd(handle: Long, fd: Int, displayName: String): String

    @JvmStatic
    external fun nativeProbeRomFd(fd: Int, displayName: String): String

    @JvmStatic
    external fun nativeListArchiveRomEntries(archivePath: String): String

    @JvmStatic
    external fun nativeExtractArchiveRomEntry(archivePath: String, entryName: String, outputPath: String): Boolean

    @JvmStatic
    external fun nativeSetSurface(handle: Long, surface: Surface?)

    @JvmStatic
    external fun nativeSetKeys(handle: Long, keys: Int)

    @JvmStatic
    external fun nativeSaveStateSlot(handle: Long, slot: Int): Boolean

    @JvmStatic
    external fun nativeLoadStateSlot(handle: Long, slot: Int): Boolean

    @JvmStatic
    external fun nativeHasStateSlot(handle: Long, slot: Int): Boolean

    @JvmStatic
    external fun nativeStateSlotModifiedMs(handle: Long, slot: Int): Long

    @JvmStatic
    external fun nativeDeleteStateSlot(handle: Long, slot: Int): Boolean

    @JvmStatic
    external fun nativeExportStateSlotFd(handle: Long, slot: Int, fd: Int): Boolean

    @JvmStatic
    external fun nativeImportStateSlotFd(handle: Long, slot: Int, fd: Int): Boolean

    @JvmStatic
    external fun nativeSaveAutoState(handle: Long): Boolean

    @JvmStatic
    external fun nativeLoadAutoState(handle: Long): Boolean

    @JvmStatic
    external fun nativeReset(handle: Long)

    @JvmStatic
    external fun nativeStepFrame(handle: Long): Boolean

    @JvmStatic
    external fun nativeSetFastForward(handle: Long, enabled: Boolean)

    @JvmStatic
    external fun nativeSetFastForwardMultiplier(handle: Long, multiplier: Int)

    @JvmStatic
    external fun nativeSetRewindConfig(handle: Long, enabled: Boolean, capacity: Int, interval: Int)

    @JvmStatic
    external fun nativeSetRewinding(handle: Long, enabled: Boolean)

    @JvmStatic
    external fun nativeSetFrameSkip(handle: Long, frames: Int)

    @JvmStatic
    external fun nativeSetAudioEnabled(handle: Long, enabled: Boolean)

    @JvmStatic
    external fun nativeSetVolumePercent(handle: Long, percent: Int)

    @JvmStatic
    external fun nativeSetAudioBufferSamples(handle: Long, samples: Int)

    @JvmStatic
    external fun nativeSetLowPassRangePercent(handle: Long, percent: Int)

    @JvmStatic
    external fun nativeRestartAudioOutput(handle: Long)

    @JvmStatic
    external fun nativeSetScaleMode(handle: Long, mode: Int)

    @JvmStatic
    external fun nativeSetFilterMode(handle: Long, mode: Int)

    @JvmStatic
    external fun nativeSetInterframeBlending(handle: Long, enabled: Boolean)

    @JvmStatic
    external fun nativeSetSkipBios(handle: Long, enabled: Boolean)

    @JvmStatic
    external fun nativeSetBiosOverridePaths(
        handle: Long,
        defaultPath: String,
        gbaPath: String,
        gbPath: String,
        gbcPath: String,
    )

    @JvmStatic
    external fun nativeSetLogLevelMode(handle: Long, mode: Int)

    @JvmStatic
    external fun nativeSetRtcMode(handle: Long, mode: Int, valueMs: Long)

    @JvmStatic
    external fun nativeGetStats(handle: Long): String

    @JvmStatic
    external fun nativeTakeScreenshot(handle: Long): String

    @JvmStatic
    external fun nativeTakeScreenshotFd(handle: Long, fd: Int): Boolean

    @JvmStatic
    external fun nativeExportBatterySave(handle: Long): String

    @JvmStatic
    external fun nativeImportBatterySaveFd(handle: Long, fd: Int): Boolean

    @JvmStatic
    external fun nativeImportPatchFd(handle: Long, fd: Int): Boolean

    @JvmStatic
    external fun nativeImportCheatsFd(handle: Long, fd: Int): Boolean

    @JvmStatic
    external fun nativePollRumble(handle: Long): Boolean

    @JvmStatic
    external fun nativeSetRotation(handle: Long, tiltX: Float, tiltY: Float, gyroZ: Float)

    @JvmStatic
    external fun nativeSetSolarLevel(handle: Long, level: Int)

    @JvmStatic
    external fun nativeSetCameraImage(handle: Long, pixels: IntArray, width: Int, height: Int): Boolean

    @JvmStatic
    external fun nativeClearCameraImage(handle: Long)

    @JvmStatic
    external fun nativeStart(handle: Long)

    @JvmStatic
    external fun nativePause(handle: Long)

    @JvmStatic
    external fun nativeResume(handle: Long)

    @JvmStatic
    external fun nativeStop(handle: Long)

    fun versionLabel(): String {
        return runCatching { nativeGetVersion() }.getOrElse { error ->
            "Unavailable (${error.javaClass.simpleName})"
        }
    }

    fun loadRomFd(handle: Long, fd: Int, displayName: String): NativeLoadResult {
        return NativeLoadResult.fromJson(
            runCatching { nativeLoadRomFd(handle, fd, displayName) }.getOrElse { error ->
                """{"ok":false,"message":"${error.javaClass.simpleName}"}"""
            },
        )
    }

    fun probeRomFd(fd: Int, displayName: String): NativeLoadResult {
        return NativeLoadResult.fromJson(
            runCatching { nativeProbeRomFd(fd, displayName) }.getOrElse { error ->
                """{"ok":false,"message":"${error.javaClass.simpleName}"}"""
            },
        )
    }

    fun archiveRomEntries(archivePath: String): List<NativeArchiveEntry> {
        return runCatching {
            val array = JSONArray(nativeListArchiveRomEntries(archivePath))
            buildList {
                for (index in 0 until array.length()) {
                    when (val item = array.opt(index)) {
                        is JSONObject -> {
                            val name = item.optString("name")
                            val displayName = item.optString("displayName").ifBlank {
                                name.substringAfterLast('/').ifBlank { name }
                            }
                            if (name.isNotBlank()) {
                                add(NativeArchiveEntry(name, displayName))
                            }
                        }
                        else -> {
                            array.optString(index).takeIf { it.isNotBlank() }?.let { name ->
                                add(NativeArchiveEntry(name, name.substringAfterLast('/').ifBlank { name }))
                            }
                        }
                    }
                }
            }
        }.getOrDefault(emptyList())
    }

    fun extractArchiveRomEntry(archivePath: String, entryName: String, outputPath: String): Boolean {
        return runCatching {
            nativeExtractArchiveRomEntry(archivePath, entryName, outputPath)
        }.getOrDefault(false)
    }

    fun stats(handle: Long): NativeStats {
        return NativeStats.fromJson(
            runCatching { nativeGetStats(handle) }.getOrDefault("{}"),
        )
    }
}
