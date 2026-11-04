
package org.mgba_emu.mgba.utils

import android.content.Context
import android.content.res.Configuration
import android.graphics.Color
import android.os.Build
import androidx.annotation.ColorInt
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.app.AppCompatDelegate
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsControllerCompat
import org.mgba_emu.mgba.MainActivity
import org.mgba_emu.mgba.R
import org.mgba_emu.mgba.settings.model.Settings
import kotlin.math.roundToInt

object ThemeHelper {
    fun setTheme(activity: AppCompatActivity) {
        when (Theme.from(Settings.theme.value)) {
            Theme.Default -> activity.setTheme(R.style.Theme_mGBA)
            Theme.Dynamic -> {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                    activity.setTheme(R.style.Theme_mGBA_Dynamic)
                } else {
                    activity.setTheme(R.style.Theme_mGBA)
                }
            }
        }
    }

    fun setCorrectTheme(activity: AppCompatActivity) {
        val currentTheme = (activity as MainActivity).themeId
        setTheme(activity)
        if (currentTheme != activity.themeId) {
            activity.recreate()
        }
    }

    fun setThemeMode(activity: AppCompatActivity) {
        val themeMode = Settings.themeMode.value
        activity.delegate.localNightMode = themeMode
        val windowController = WindowCompat.getInsetsController(
            activity.window,
            activity.window.decorView
        )
        when (themeMode) {
            AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM -> when (isNightMode(activity)) {
                false -> setLightModeSystemBars(windowController)
                true -> setDarkModeSystemBars(windowController)
            }
            AppCompatDelegate.MODE_NIGHT_NO -> setLightModeSystemBars(windowController)
            AppCompatDelegate.MODE_NIGHT_YES -> setDarkModeSystemBars(windowController)
        }
    }

    private fun isNightMode(activity: AppCompatActivity): Boolean {
        return when (activity.resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) {
            Configuration.UI_MODE_NIGHT_NO -> false
            Configuration.UI_MODE_NIGHT_YES -> true
            else -> false
        }
    }

    private fun setLightModeSystemBars(windowController: WindowInsetsControllerCompat) {
        windowController.isAppearanceLightStatusBars = true
        windowController.isAppearanceLightNavigationBars = true
    }

    private fun setDarkModeSystemBars(windowController: WindowInsetsControllerCompat) {
        windowController.isAppearanceLightStatusBars = false
        windowController.isAppearanceLightNavigationBars = false
    }

    fun getColorWithOpacity(@ColorInt color: Int, alphaFactor: Float): Int {
        return Color.argb(
            (alphaFactor * Color.alpha(color)).roundToInt(),
            Color.red(color),
            Color.green(color),
            Color.blue(color)
        )
    }
}

enum class Theme(val int: Int) {
    Default(0),
    Dynamic(1);

    companion object {
        fun from(int: Int): Theme = entries.firstOrNull { it.int == int } ?: Default
    }
}