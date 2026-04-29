package io.mgba.android.emulator

import android.content.Context

object EmulatorSession {
    private var controller: EmulatorController? = null

    fun controller(context: Context): EmulatorController {
        return controller ?: EmulatorController(context).also { controller = it }
    }

    fun current(): EmulatorController? {
        return controller
    }

    fun close() {
        controller?.close()
        controller = null
    }
}
