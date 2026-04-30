package io.mgba.android.settings

object FastForwardModes {
    const val ModeToggle = 0
    const val ModeHold = 1
    const val Multiplier1x = 100
    const val MultiplierDefault = 200

    val modeLabels = arrayOf("Toggle", "Hold")
    val multiplierValues = intArrayOf(Multiplier1x, 125, 150, 200, 300, 400, 800)
    val multiplierLabels = arrayOf("1x", "1.25x", "1.5x", "2x", "3x", "4x", "8x")

    fun coerceMode(mode: Int): Int {
        return mode.coerceIn(0, modeLabels.lastIndex)
    }

    fun coerceMultiplier(multiplier: Int): Int {
        return when (multiplier) {
            0 -> MultiplierDefault
            1 -> Multiplier1x
            2 -> 200
            3 -> 300
            4 -> 400
            in multiplierValues -> multiplier
            else -> MultiplierDefault
        }
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
