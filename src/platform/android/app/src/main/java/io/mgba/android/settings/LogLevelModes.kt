package io.mgba.android.settings

object LogLevelModes {
    const val ModeWarn = 0
    const val ModeInfo = 1
    const val ModeDebug = 2

    val labels = arrayOf("Warn", "Info", "Debug")

    fun coerce(mode: Int): Int {
        return mode.coerceIn(ModeWarn, ModeDebug)
    }

    fun next(mode: Int): Int {
        return (coerce(mode) + 1) % labels.size
    }
}
