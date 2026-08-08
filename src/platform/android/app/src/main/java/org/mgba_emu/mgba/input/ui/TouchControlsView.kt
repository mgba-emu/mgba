
package org.mgba_emu.mgba.input.ui

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.View
import org.mgba_emu.mgba.input.GbaKey
import org.mgba_emu.mgba.input.InputState
import kotlin.math.abs
import kotlin.math.hypot

class TouchControlsView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : View(context, attrs) {

    lateinit var inputState: InputState

    private val paintButton = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(120, 255, 255, 255)
        style = Paint.Style.FILL
    }
    private val paintButtonPressed = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(200, 255, 255, 255)
        style = Paint.Style.FILL
    }
    private val paintLabel = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(220, 20, 20, 20)
        textAlign = Paint.Align.CENTER
        textSize = 36f
    }

    // Dpad
    private var dpadCenterX = 0f
    private var dpadCenterY = 0f
    private var dpadArmLength = 0f
    private var dpadArmWidth = 0f

    // face buttons
    private data class FaceButton(val key: Int, val label: String, val rect: RectF)
    private val faceButtons = mutableListOf<FaceButton>()

    // start/select
    private data class SmallButton(val key: Int, val label: String, val rect: RectF)
    private val smallButtons = mutableListOf<SmallButton>()

    // shoulder buttons
    private val shoulderButtons = mutableListOf<SmallButton>()

    private val pointerKeys = mutableMapOf<Int, Int>() // pointerId -> keyMask held by that pointer

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        layoutControls(w, h)
    }

    private fun layoutControls(w: Int, h: Int) {
        val margin = w * 0.04f

        // Dpad
        dpadArmLength = w * 0.11f
        dpadArmWidth = w * 0.09f
        dpadCenterX = margin + dpadArmLength
        dpadCenterY = h - margin - dpadArmLength

        // face buttons
        val btnRadius = w * 0.055f
        val faceCenterX = w - margin - btnRadius * 2.2f
        val faceCenterY = h - margin - btnRadius * 2.2f
        faceButtons.clear()
        faceButtons += FaceButton(
            GbaKey.A, "A",
            RectF(
                faceCenterX + btnRadius * 1.3f - btnRadius, faceCenterY - btnRadius,
                faceCenterX + btnRadius * 1.3f + btnRadius, faceCenterY + btnRadius
            )
        )
        faceButtons += FaceButton(
            GbaKey.B, "B",
            RectF(
                faceCenterX - btnRadius, faceCenterY + btnRadius * 1.3f - btnRadius,
                faceCenterX + btnRadius, faceCenterY + btnRadius * 1.3f + btnRadius
            )
        )

        // start/select
        val smallW = w * 0.09f
        val smallH = h * 0.045f
        val centerX = w / 2f
        smallButtons.clear()
        smallButtons += SmallButton(
            GbaKey.SELECT, "SELECT",
            RectF(centerX - smallW * 1.1f, h - margin - smallH, centerX - smallW * 0.1f, h - margin)
        )
        smallButtons += SmallButton(
            GbaKey.START, "START",
            RectF(centerX + smallW * 0.1f, h - margin - smallH, centerX + smallW * 1.1f, h - margin)
        )

        // shoulder L/R
        val shoulderW = w * 0.13f
        val shoulderH = h * 0.06f
        shoulderButtons.clear()
        shoulderButtons += SmallButton(
            GbaKey.L, "L",
            RectF(margin, margin, margin + shoulderW, margin + shoulderH)
        )
        shoulderButtons += SmallButton(
            GbaKey.R, "R",
            RectF(w - margin - shoulderW, margin, w - margin, margin + shoulderH)
        )
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        drawDpad(canvas)
        faceButtons.forEach { drawCircleButton(canvas, it.rect, it.label, isKeyHeld(it.key)) }
        smallButtons.forEach { drawRectButton(canvas, it.rect, it.label, isKeyHeld(it.key)) }
        shoulderButtons.forEach { drawRectButton(canvas, it.rect, it.label, isKeyHeld(it.key)) }
    }

    private fun isKeyHeld(key: Int): Boolean = pointerKeys.values.any { it and key != 0 }

    private fun drawDpad(canvas: Canvas) {
        val paint = paintButton
        canvas.drawRect(
            dpadCenterX - dpadArmWidth / 2, dpadCenterY - dpadArmLength,
            dpadCenterX + dpadArmWidth / 2, dpadCenterY + dpadArmLength,
            if (isKeyHeld(GbaKey.UP) || isKeyHeld(GbaKey.DOWN)) paintButtonPressed else paint
        )
        canvas.drawRect(
            dpadCenterX - dpadArmLength, dpadCenterY - dpadArmWidth / 2,
            dpadCenterX + dpadArmLength, dpadCenterY + dpadArmWidth / 2,
            if (isKeyHeld(GbaKey.LEFT) || isKeyHeld(GbaKey.RIGHT)) paintButtonPressed else paint
        )
    }

    private fun drawCircleButton(canvas: Canvas, rect: RectF, label: String, pressed: Boolean) {
        val radius = rect.width() / 2
        canvas.drawCircle(rect.centerX(), rect.centerY(), radius, if (pressed) paintButtonPressed else paintButton)
        canvas.drawText(label, rect.centerX(), rect.centerY() + paintLabel.textSize / 3, paintLabel)
    }

    private fun drawRectButton(canvas: Canvas, rect: RectF, label: String, pressed: Boolean) {
        canvas.drawRoundRect(rect, 12f, 12f, if (pressed) paintButtonPressed else paintButton)
        canvas.drawText(label, rect.centerX(), rect.centerY() + paintLabel.textSize / 3, paintLabel)
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                val idx = event.actionIndex
                handlePointerDown(event.getPointerId(idx), event.getX(idx), event.getY(idx))
            }
            MotionEvent.ACTION_MOVE -> {
                for (i in 0 until event.pointerCount) {
                    handlePointerMove(event.getPointerId(i), event.getX(i), event.getY(i))
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                val idx = event.actionIndex
                handlePointerUp(event.getPointerId(idx))
            }
            MotionEvent.ACTION_CANCEL -> {
                pointerKeys.keys.toList().forEach { handlePointerUp(it) }
            }
        }
        invalidate()
        return true
    }

    private fun handlePointerDown(pointerId: Int, x: Float, y: Float) {
        val keys = resolveKeysAt(x, y)
        applyPointerKeys(pointerId, keys)
    }

    private fun handlePointerMove(pointerId: Int, x: Float, y: Float) {
        val keys = resolveKeysAt(x, y)
        applyPointerKeys(pointerId, keys)
    }

    private fun handlePointerUp(pointerId: Int) {
        val previous = pointerKeys.remove(pointerId) ?: 0
        if (previous != 0) {
            releaseIfNotHeldElsewhere(previous, excludePointer = pointerId)
        }
    }

    private fun applyPointerKeys(pointerId: Int, newKeys: Int) {
        val previous = pointerKeys[pointerId] ?: 0
        if (previous == newKeys) return

        val released = previous and newKeys.inv()
        val pressed = newKeys and previous.inv()

        pointerKeys[pointerId] = newKeys

        if (released != 0) releaseIfNotHeldElsewhere(released, excludePointer = pointerId)
        if (pressed != 0) {
            var bit = 1
            while (bit <= GbaKey.R) {
                if (pressed and bit != 0) inputState.press(bit)
                bit = bit shl 1
            }
        }
    }

    private fun releaseIfNotHeldElsewhere(keys: Int, excludePointer: Int) {
        var bit = 1
        while (bit <= GbaKey.R) {
            if (keys and bit != 0) {
                val stillHeld = pointerKeys.entries.any { (pid, mask) -> pid != excludePointer && mask and bit != 0 }
                if (!stillHeld) inputState.release(bit)
            }
            bit = bit shl 1
        }
    }

    private fun resolveKeysAt(x: Float, y: Float): Int {
        var keys = 0

        val dx = x - dpadCenterX
        val dy = y - dpadCenterY
        val dist = hypot(dx.toDouble(), dy.toDouble()).toFloat()
        if (dist <= dpadArmLength + dpadArmWidth / 2) {
            val withinVerticalArm = abs(dx) <= dpadArmWidth
            val withinHorizontalArm = abs(dy) <= dpadArmWidth
            if (withinVerticalArm && dy < -dpadArmWidth / 2) keys = keys or GbaKey.UP
            if (withinVerticalArm && dy > dpadArmWidth / 2) keys = keys or GbaKey.DOWN
            if (withinHorizontalArm && dx < -dpadArmWidth / 2) keys = keys or GbaKey.LEFT
            if (withinHorizontalArm && dx > dpadArmWidth / 2) keys = keys or GbaKey.RIGHT
            if (!withinVerticalArm && !withinHorizontalArm) {
                keys = keys or (if (dy < 0) GbaKey.UP else GbaKey.DOWN)
                keys = keys or (if (dx < 0) GbaKey.LEFT else GbaKey.RIGHT)
            }
        }

        faceButtons.forEach { if (it.rect.contains(x, y)) keys = keys or it.key }
        smallButtons.forEach { if (it.rect.contains(x, y)) keys = keys or it.key }
        shoulderButtons.forEach { if (it.rect.contains(x, y)) keys = keys or it.key }

        return keys
    }
}