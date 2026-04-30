package io.mgba.android.settings

object FastForwardModes {
    const val ModeToggle = 0
    const val ModeHold = 1
    const val MultiplierMax = 0
    const val MultiplierDefault = 2

    val modeLabels = arrayOf("Toggle", "Hold")
    val multiplierValues = intArrayOf(MultiplierMax, 2, 3, 4)
    val multiplierLabels = arrayOf("Max", "2x", "3x", "4x")

    fun coerceMode(mode: Int): Int {
        return mode.coerceIn(0, modeLabels.lastIndex)
    }

    fun coerceMultiplier(multiplier: Int): Int {
        return if (multiplier in multiplierValues) multiplier else MultiplierMax
    }

    fun nextMultiplier(multiplier: Int): Int {
        val index = multiplierValues.indexOf(coerceMultiplier(multiplier)).takeIf { it >= 0 } ?: 0
        return multiplierValues[(index + 1) % multiplierValues.size]
    }

    fun labelForMultiplier(multiplier: Int): String {
        val index = multiplierValues.indexOf(coerceMultiplier(multiplier)).takeIf { it >= 0 } ?: 0
        return multiplierLabels[index]
    }
}
