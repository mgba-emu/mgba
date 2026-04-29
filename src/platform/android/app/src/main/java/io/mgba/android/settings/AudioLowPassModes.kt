package io.mgba.android.settings

object AudioLowPassModes {
    val labels = arrayOf("LPFOff", "LPF40", "LPF60", "LPF80")
    private val names = arrayOf("Off", "40%", "60%", "80%")
    private val ranges = arrayOf(0, 40, 60, 80)

    fun rangeFor(mode: Int): Int {
        return ranges[mode.coerceIn(ranges.indices)]
    }

    fun labelFor(mode: Int): String {
        return labels[mode.coerceIn(labels.indices)]
    }

    fun nameFor(mode: Int): String {
        return names[mode.coerceIn(names.indices)]
    }
}
