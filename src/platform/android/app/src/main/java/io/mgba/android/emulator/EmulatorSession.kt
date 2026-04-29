package io.mgba.android.emulator

import android.content.Context

data class CurrentGame(
    val uri: String,
    val displayName: String,
    val stableId: String,
    val crc32: String,
    val sha1: String,
    val launchStartedAtMs: Long,
    val loadedAtMs: Long,
)

object EmulatorSession {
    private var controller: EmulatorController? = null
    private var currentGame: CurrentGame? = null

    fun controller(context: Context): EmulatorController {
        return controller ?: EmulatorController(context).also { controller = it }
    }

    fun setCurrentGame(
        uri: String,
        displayName: String,
        stableId: String = uri,
        crc32: String = "",
        sha1: String = "",
        launchStartedAtMs: Long = 0L,
        loadedAtMs: Long = 0L,
    ) {
        currentGame = CurrentGame(uri, displayName, stableId, crc32, sha1, launchStartedAtMs, loadedAtMs)
    }

    fun currentGame(): CurrentGame? {
        return currentGame
    }

    fun current(): EmulatorController? {
        return controller
    }

    fun close() {
        controller?.close()
        controller = null
        currentGame = null
    }
}
