package io.mgba.android.input

import android.view.InputDevice
import android.view.MotionEvent

object AndroidInputMapper {
    const val DefaultAxisThresholdPercent = 45

    fun keyMaskForKeyCode(keyCode: Int): Int {
        return keyMaskForKeyCode(keyCode, HardwareKeyProfile.defaultProfile())
    }

    fun keyMaskForKeyCode(keyCode: Int, profile: HardwareKeyProfile): Int {
        return profile.maskForKeyCode(keyCode)
    }

    fun motionKeys(event: MotionEvent): Int {
        return motionKeys(event, DefaultAxisThresholdPercent)
    }

    fun motionKeys(event: MotionEvent, thresholdPercent: Int): Int {
        return motionKeysForAxes(
            event.source,
            event.getAxisValue(MotionEvent.AXIS_X),
            event.getAxisValue(MotionEvent.AXIS_Y),
            event.getAxisValue(MotionEvent.AXIS_HAT_X),
            event.getAxisValue(MotionEvent.AXIS_HAT_Y),
            event.getAxisValue(MotionEvent.AXIS_LTRIGGER),
            event.getAxisValue(MotionEvent.AXIS_BRAKE),
            event.getAxisValue(MotionEvent.AXIS_RTRIGGER),
            event.getAxisValue(MotionEvent.AXIS_GAS),
            thresholdPercent,
        )
    }

    fun motionKeysForAxes(
        source: Int,
        xAxis: Float,
        yAxis: Float,
        hatXAxis: Float,
        hatYAxis: Float,
        leftTriggerAxis: Float,
        brakeAxis: Float,
        rightTriggerAxis: Float,
        gasAxis: Float,
        thresholdPercent: Int,
    ): Int {
        if ((source and InputDevice.SOURCE_JOYSTICK) != InputDevice.SOURCE_JOYSTICK &&
            (source and InputDevice.SOURCE_GAMEPAD) != InputDevice.SOURCE_GAMEPAD
        ) {
            return 0
        }

        var keys = 0
        val threshold = (thresholdPercent.coerceIn(10, 90) / 100f)
        val x = strongestValue(xAxis, hatXAxis)
        val y = strongestValue(yAxis, hatYAxis)
        val leftTrigger = strongestValue(leftTriggerAxis, brakeAxis)
        val rightTrigger = strongestValue(rightTriggerAxis, gasAxis)
        if (x <= -threshold) {
            keys = keys or GbaKeyMask.Left
        } else if (x >= threshold) {
            keys = keys or GbaKeyMask.Right
        }
        if (y <= -threshold) {
            keys = keys or GbaKeyMask.Up
        } else if (y >= threshold) {
            keys = keys or GbaKeyMask.Down
        }
        if (leftTrigger >= threshold) {
            keys = keys or GbaKeyMask.L
        }
        if (rightTrigger >= threshold) {
            keys = keys or GbaKeyMask.R
        }
        return keys
    }

    private fun strongestValue(primary: Float, secondary: Float): Float {
        return if (kotlin.math.abs(primary) >= kotlin.math.abs(secondary)) {
            primary
        } else {
            secondary
        }
    }
}
