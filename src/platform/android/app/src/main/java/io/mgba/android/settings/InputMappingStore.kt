package io.mgba.android.settings

import android.content.Context
import io.mgba.android.input.GbaButtons
import io.mgba.android.input.HardwareKeyProfile

class InputMappingStore(context: Context) {
    private val preferences = context.applicationContext.getSharedPreferences("input_mappings", Context.MODE_PRIVATE)

    fun profile(gameId: String?): HardwareKeyProfile {
        var profile = HardwareKeyProfile.defaultProfile()
        GbaButtons.All.forEach { button ->
            val key = keyForScope(GLOBAL_SCOPE, button.mask)
            if (preferences.contains(key)) {
                profile = profile.withKeyCode(button.mask, preferences.getInt(key, 0))
            }
        }

        val gameScope = scope(gameId)
        if (gameScope != GLOBAL_SCOPE) {
            GbaButtons.All.forEach { button ->
                val key = keyForScope(gameScope, button.mask)
                if (preferences.contains(key)) {
                    profile = profile.withKeyCode(button.mask, preferences.getInt(key, 0))
                }
            }
        }
        return profile
    }

    fun setKeyCode(gameId: String?, mask: Int, keyCode: Int): Boolean {
        if (!HardwareKeyProfile.isSupportedMask(mask)) {
            return false
        }
        val scope = scope(gameId)
        val editor = preferences.edit()
        GbaButtons.All.forEach { button ->
            val key = keyForScope(scope, button.mask)
            if (button.mask != mask && preferences.getInt(key, Int.MIN_VALUE) == keyCode) {
                editor.remove(key)
            }
        }
        editor.putInt(keyForScope(scope, mask), keyCode).apply()
        return true
    }

    fun reset(gameId: String?) {
        val scope = scope(gameId)
        val editor = preferences.edit()
        GbaButtons.All.forEach { button ->
            editor.remove(keyForScope(scope, button.mask))
        }
        editor.apply()
    }

    private fun scope(gameId: String?): String {
        return if (gameId.isNullOrBlank()) GLOBAL_SCOPE else gameId
    }

    private fun keyForScope(scope: String, mask: Int): String {
        return "$scope::$mask"
    }

    private companion object {
        const val GLOBAL_SCOPE = "global"
    }
}
