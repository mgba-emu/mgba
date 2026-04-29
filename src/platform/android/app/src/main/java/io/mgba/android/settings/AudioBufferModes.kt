package io.mgba.android.settings

object AudioBufferModes {
    val labels = arrayOf("BufLow", "BufBal", "BufStb")
    private val names = arrayOf("Low Latency", "Balanced", "Stable")
    private val samples = arrayOf(512, 2048, 4096)

    fun samplesFor(mode: Int): Int {
        return samples[mode.coerceIn(samples.indices)]
    }

    fun labelFor(mode: Int): String {
        return labels[mode.coerceIn(labels.indices)]
    }

    fun nameFor(mode: Int): String {
        return names[mode.coerceIn(names.indices)]
    }
}
