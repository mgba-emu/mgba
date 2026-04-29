package io.mgba.android.input

import android.view.InputDevice
import android.view.MotionEvent

object AndroidInputMapper {
    private const val AxisThreshold = 0.45f

    fun keyMaskForKeyCode(keyCode: Int): Int {
        return keyMaskForKeyCode(keyCode, HardwareKeyProfile.defaultProfile())
    }

    fun keyMaskForKeyCode(keyCode: Int, profile: HardwareKeyProfile): Int {
        return profile.maskForKeyCode(keyCode)
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
