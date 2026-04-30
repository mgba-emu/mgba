package io.mgba.android.settings

object AutoStateSettings {
    const val DefaultIntervalSeconds = 10
    const val MinIntervalSeconds = 5
    const val MaxIntervalSeconds = 600

    fun coerceIntervalSeconds(seconds: Int): Int {
        return seconds.coerceIn(MinIntervalSeconds, MaxIntervalSeconds)
    }

    fun labelForInterval(seconds: Int): String {
        return "${coerceIntervalSeconds(seconds)}s"
    }
}
