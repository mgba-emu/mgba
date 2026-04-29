package io.mgba.android.emulator

import android.content.Context

data class CurrentGame(
    val uri: String,
    val displayName: String,
)

object EmulatorSession {
    private var controller: EmulatorController? = null
    private var currentGame: CurrentGame? = null

    fun controller(context: Context): EmulatorController {
        return controller ?: EmulatorController(context).also { controller = it }
    }

    fun setCurrentGame(uri: String, displayName: String) {
        currentGame = CurrentGame(uri, displayName)
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
