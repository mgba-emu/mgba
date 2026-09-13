
package org.mgba_emu.mgba.utils

import android.graphics.Color
import androidx.annotation.ColorInt
import kotlin.math.roundToInt

object ThemeHelper {
    fun getColorWithOpacity(@ColorInt color: Int, alphaFactor: Float): Int {
        return Color.argb(
            (alphaFactor * Color.alpha(color)).roundToInt(),
            Color.red(color),
            Color.green(color),
            Color.blue(color)
        )
    }
}