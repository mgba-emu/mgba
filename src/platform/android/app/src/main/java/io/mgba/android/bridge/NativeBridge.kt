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
    external fun nativeSetSurface(handle: Long, surface: Surface?)

    @JvmStatic
    external fun nativeSetKeys(handle: Long, keys: Int)

    @JvmStatic
    external fun nativeSaveStateSlot(handle: Long, slot: Int): Boolean

    @JvmStatic
    external fun nativeLoadStateSlot(handle: Long, slot: Int): Boolean

    @JvmStatic
    external fun nativeReset(handle: Long)

    @JvmStatic
    external fun nativeSetFastForward(handle: Long, enabled: Boolean)

    @JvmStatic
    external fun nativeTakeScreenshot(handle: Long): String

    @JvmStatic
    external fun nativeExportBatterySave(handle: Long): String

    @JvmStatic
    external fun nativeImportBatterySaveFd(handle: Long, fd: Int): Boolean

    @JvmStatic
    external fun nativeImportCheatsFd(handle: Long, fd: Int): Boolean

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
}
