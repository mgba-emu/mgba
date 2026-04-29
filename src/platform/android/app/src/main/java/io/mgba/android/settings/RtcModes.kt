package io.mgba.android.settings

object RtcModes {
    const val ModeWallClock = 0
    const val ModeFixed = 1
    const val ModeFakeEpoch = 2
    const val ModeWallClockOffset = 3
    const val DefaultFixedTimeMs = 946684800000L

    val labels = arrayOf("Clock", "Fixed", "Fake", "Offset")

    fun coerce(mode: Int): Int {
        return mode.coerceIn(ModeWallClock, ModeWallClockOffset)
    }

    fun next(mode: Int): Int {
        return (coerce(mode) + 1) % labels.size
    }
}
