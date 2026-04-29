package io.mgba.android.settings

object RewindSettings {
    val bufferCapacities = intArrayOf(300, 600, 900, 1200)
    val bufferIntervals = intArrayOf(1, 2, 3, 4)

    fun coerceCapacity(capacity: Int): Int {
        return if (capacity in bufferCapacities) capacity else 600
    }

    fun coerceInterval(interval: Int): Int {
        return if (interval in bufferIntervals) interval else 1
    }

    fun nextCapacity(capacity: Int): Int {
        val index = bufferCapacities.indexOf(coerceCapacity(capacity)).takeIf { it >= 0 } ?: 1
        return bufferCapacities[(index + 1) % bufferCapacities.size]
    }

    fun nextInterval(interval: Int): Int {
        val index = bufferIntervals.indexOf(coerceInterval(interval)).takeIf { it >= 0 } ?: 0
        return bufferIntervals[(index + 1) % bufferIntervals.size]
    }
}
