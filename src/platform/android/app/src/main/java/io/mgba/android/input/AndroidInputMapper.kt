package io.mgba.android.input

import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent

object AndroidInputMapper {
    private const val AxisThreshold = 0.45f

    fun keyMaskForKeyCode(keyCode: Int): Int {
        return when (keyCode) {
            KeyEvent.KEYCODE_BUTTON_A,
            KeyEvent.KEYCODE_X -> GbaKeyMask.A
            KeyEvent.KEYCODE_BUTTON_B,
            KeyEvent.KEYCODE_Z,
            KeyEvent.KEYCODE_C -> GbaKeyMask.B
            KeyEvent.KEYCODE_BUTTON_SELECT,
            KeyEvent.KEYCODE_TAB -> GbaKeyMask.Select
            KeyEvent.KEYCODE_BUTTON_START,
            KeyEvent.KEYCODE_ENTER -> GbaKeyMask.Start
            KeyEvent.KEYCODE_DPAD_RIGHT,
            KeyEvent.KEYCODE_RIGHT_BRACKET -> GbaKeyMask.Right
            KeyEvent.KEYCODE_DPAD_LEFT,
            KeyEvent.KEYCODE_LEFT_BRACKET -> GbaKeyMask.Left
            KeyEvent.KEYCODE_DPAD_UP,
            KeyEvent.KEYCODE_I -> GbaKeyMask.Up
            KeyEvent.KEYCODE_DPAD_DOWN,
            KeyEvent.KEYCODE_K -> GbaKeyMask.Down
            KeyEvent.KEYCODE_BUTTON_R1,
            KeyEvent.KEYCODE_E -> GbaKeyMask.R
            KeyEvent.KEYCODE_BUTTON_L1,
            KeyEvent.KEYCODE_Q -> GbaKeyMask.L
            else -> 0
        }
    }

    fun motionKeys(event: MotionEvent): Int {
        val source = event.source
        if ((source and InputDevice.SOURCE_JOYSTICK) != InputDevice.SOURCE_JOYSTICK &&
            (source and InputDevice.SOURCE_GAMEPAD) != InputDevice.SOURCE_GAMEPAD
        ) {
            return 0
        }

        var keys = 0
        val x = strongestAxis(event, MotionEvent.AXIS_X, MotionEvent.AXIS_HAT_X)
        val y = strongestAxis(event, MotionEvent.AXIS_Y, MotionEvent.AXIS_HAT_Y)
        if (x <= -AxisThreshold) {
            keys = keys or GbaKeyMask.Left
        } else if (x >= AxisThreshold) {
            keys = keys or GbaKeyMask.Right
        }
        if (y <= -AxisThreshold) {
            keys = keys or GbaKeyMask.Up
        } else if (y >= AxisThreshold) {
            keys = keys or GbaKeyMask.Down
        }
        return keys
    }

    private fun strongestAxis(event: MotionEvent, primary: Int, secondary: Int): Float {
        val primaryValue = event.getAxisValue(primary)
        val secondaryValue = event.getAxisValue(secondary)
        return if (kotlin.math.abs(primaryValue) >= kotlin.math.abs(secondaryValue)) {
            primaryValue
        } else {
            secondaryValue
        }
    }
}
