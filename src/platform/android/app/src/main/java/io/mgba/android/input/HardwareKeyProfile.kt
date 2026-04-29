package io.mgba.android.input

import android.view.KeyEvent

data class HardwareKeyProfile(
    private val customKeyCodesByMask: Map<Int, Int> = emptyMap(),
) {
    fun maskForKeyCode(keyCode: Int): Int {
        var customMask = 0
        customKeyCodesByMask.forEach { (mask, customKeyCode) ->
            if (customKeyCode == keyCode) {
                customMask = customMask or mask
            }
        }
        if (customMask != 0) {
            return customMask
        }

        var mask = 0
        GbaButtons.All.forEach { button ->
            if (!customKeyCodesByMask.containsKey(button.mask) &&
                DefaultKeyCodesByMask[button.mask]?.contains(keyCode) == true
            ) {
                mask = mask or button.mask
            }
        }
        return mask
    }

    fun keyCodeForMask(mask: Int): Int? {
        return customKeyCodesByMask[mask] ?: DefaultKeyCodesByMask[mask]?.firstOrNull()
    }

    fun withKeyCode(mask: Int, keyCode: Int): HardwareKeyProfile {
        if (!isSupportedMask(mask)) {
            return this
        }
        val next = customKeyCodesByMask
            .filter { (mappedMask, mappedKeyCode) -> mappedMask != mask && mappedKeyCode != keyCode }
            .toMutableMap()
        next[mask] = keyCode
        return copy(customKeyCodesByMask = next)
    }

    companion object {
        private val DefaultKeyCodesByMask = mapOf(
            GbaKeyMask.A to listOf(KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_X),
            GbaKeyMask.B to listOf(KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_Z, KeyEvent.KEYCODE_C),
            GbaKeyMask.Select to listOf(KeyEvent.KEYCODE_BUTTON_SELECT, KeyEvent.KEYCODE_TAB),
            GbaKeyMask.Start to listOf(KeyEvent.KEYCODE_BUTTON_START, KeyEvent.KEYCODE_ENTER),
            GbaKeyMask.Right to listOf(KeyEvent.KEYCODE_DPAD_RIGHT, KeyEvent.KEYCODE_RIGHT_BRACKET),
            GbaKeyMask.Left to listOf(KeyEvent.KEYCODE_DPAD_LEFT, KeyEvent.KEYCODE_LEFT_BRACKET),
            GbaKeyMask.Up to listOf(KeyEvent.KEYCODE_DPAD_UP, KeyEvent.KEYCODE_I),
            GbaKeyMask.Down to listOf(KeyEvent.KEYCODE_DPAD_DOWN, KeyEvent.KEYCODE_K),
            GbaKeyMask.R to listOf(KeyEvent.KEYCODE_BUTTON_R1, KeyEvent.KEYCODE_E),
            GbaKeyMask.L to listOf(KeyEvent.KEYCODE_BUTTON_L1, KeyEvent.KEYCODE_Q),
        )

        @JvmStatic
        fun defaultProfile(): HardwareKeyProfile {
            return HardwareKeyProfile()
        }

        @JvmStatic
        fun isSupportedMask(mask: Int): Boolean {
            return GbaButtons.All.any { it.mask == mask }
        }
    }
}
