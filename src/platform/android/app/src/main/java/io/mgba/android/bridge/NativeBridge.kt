package io.mgba.android.bridge

import android.view.Surface

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
    external fun nativeDeleteStateSlot(handle: Long, slot: Int): Boolean

    @JvmStatic
    external fun nativeExportStateSlotFd(handle: Long, slot: Int, fd: Int): Boolean

    @JvmStatic
    external fun nativeImportStateSlotFd(handle: Long, slot: Int, fd: Int): Boolean

    @JvmStatic
    external fun nativeReset(handle: Long)

    @JvmStatic
    external fun nativeSetFastForward(handle: Long, enabled: Boolean)

    @JvmStatic
    external fun nativeSetAudioEnabled(handle: Long, enabled: Boolean)

    @JvmStatic
    external fun nativeSetScaleMode(handle: Long, mode: Int)

    @JvmStatic
    external fun nativeGetStats(handle: Long): String

    @JvmStatic
    external fun nativeTakeScreenshot(handle: Long): String

    @JvmStatic
    external fun nativeExportBatterySave(handle: Long): String

    @JvmStatic
    external fun nativeImportBatterySaveFd(handle: Long, fd: Int): Boolean

    @JvmStatic
    external fun nativeImportCheatsFd(handle: Long, fd: Int): Boolean

    @JvmStatic
    external fun nativePollRumble(handle: Long): Boolean

    @JvmStatic
    external fun nativeSetRotation(handle: Long, tiltX: Float, tiltY: Float, gyroZ: Float)

    @JvmStatic
    external fun nativeSetSolarLevel(handle: Long, level: Int)

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

    fun stats(handle: Long): NativeStats {
        return NativeStats.fromJson(
            runCatching { nativeGetStats(handle) }.getOrDefault("{}"),
        )
    }
}
