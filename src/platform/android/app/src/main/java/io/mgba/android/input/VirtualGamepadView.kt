package io.mgba.android.input

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.view.MotionEvent
import android.view.View
import android.view.HapticFeedbackConstants
import kotlin.math.min

class VirtualGamepadView(context: Context) : View(context) {
    private enum class Shape {
        Circle,
        RoundRect,
    }

    private data class Region(
        val label: String,
        val mask: Int,
        val bounds: RectF,
        val shape: Shape,
    )

    private val regions = mutableListOf<Region>()
    private val fillPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val strokePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = dp(1.5f)
        color = Color.argb(170, 255, 255, 255)
    }
    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textAlign = Paint.Align.CENTER
        typeface = android.graphics.Typeface.DEFAULT_BOLD
    }
    private var pressedKeys = 0
    private var onKeysChanged: ((Int) -> Unit)? = null
    private var sizePercent = 100
    private var opacityPercent = 100

    init {
        isFocusable = true
        isClickable = true
        setWillNotDraw(false)
    }

    fun setOnKeysChangedListener(listener: (Int) -> Unit) {
        onKeysChanged = listener
    }

    fun clearKeys() {
        updateKeys(0)
    }

    fun setStyle(sizePercent: Int, opacityPercent: Int) {
        val newSizePercent = sizePercent.coerceIn(60, 140)
        val newOpacityPercent = opacityPercent.coerceIn(35, 100)
        if (this.sizePercent == newSizePercent && this.opacityPercent == newOpacityPercent) {
            return
        }
        this.sizePercent = newSizePercent
        this.opacityPercent = newOpacityPercent
        rebuildRegions(width, height)
        invalidate()
    }

    override fun onSizeChanged(width: Int, height: Int, oldWidth: Int, oldHeight: Int) {
        rebuildRegions(width, height)
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (event.actionMasked == MotionEvent.ACTION_CANCEL) {
            performClick()
            updateKeys(0)
            return true
        }

        var keys = 0
        val liftedPointer = when (event.actionMasked) {
            MotionEvent.ACTION_UP,
            MotionEvent.ACTION_POINTER_UP -> event.actionIndex
            else -> -1
        }

        for (index in 0 until event.pointerCount) {
            if (index == liftedPointer) {
                continue
            }
            keys = keys or keysAt(event.getX(index), event.getY(index))
        }

        if (event.actionMasked == MotionEvent.ACTION_UP || event.actionMasked == MotionEvent.ACTION_POINTER_UP) {
            performClick()
        }

        updateKeys(keys)
        return true
    }

    override fun performClick(): Boolean {
        super.performClick()
        return true
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (regions.isEmpty()) {
            rebuildRegions(width, height)
        }

        for (region in regions) {
            val active = pressedKeys and region.mask != 0
            val opacity = opacityPercent / 100f
            fillPaint.style = Paint.Style.FILL
            fillPaint.color = if (active) {
                Color.argb((210 * opacity).toInt().coerceIn(45, 255), 88, 180, 255)
            } else {
                Color.argb((92 * opacity).toInt().coerceIn(24, 255), 255, 255, 255)
            }
            strokePaint.color = Color.argb((170 * opacity).toInt().coerceIn(40, 255), 255, 255, 255)
            textPaint.color = Color.argb((255 * opacity).toInt().coerceIn(90, 255), 255, 255, 255)
            drawRegion(canvas, region, fillPaint)
            drawRegion(canvas, region, strokePaint)

            textPaint.textSize = if (region.label.length > 1) dp(11f) else dp(16f)
            val centerY = region.bounds.centerY() - (textPaint.descent() + textPaint.ascent()) / 2f
            canvas.drawText(region.label, region.bounds.centerX(), centerY, textPaint)
        }
    }

    private fun rebuildRegions(width: Int, height: Int) {
        regions.clear()
        if (width <= 0 || height <= 0) {
            return
        }

        val base = min(width, height).toFloat()
        val scale = sizePercent / 100f
        val button = base.times(0.108f).times(scale).coerceIn(dp(36f), dp(104f))
        val smallButton = button * 0.86f
        val padding = dp(24f) * scale
        val controlsY = height - padding - button * 1.65f
        val dpadX = padding + button * 1.75f
        val faceX = width - padding - button * 1.65f

        addCircle("UP", GbaKeyMask.Up, dpadX, controlsY - button, smallButton)
        addCircle("DOWN", GbaKeyMask.Down, dpadX, controlsY + button, smallButton)
        addCircle("LEFT", GbaKeyMask.Left, dpadX - button, controlsY, smallButton)
        addCircle("RIGHT", GbaKeyMask.Right, dpadX + button, controlsY, smallButton)

        addCircle("B", GbaKeyMask.B, faceX - button * 1.05f, controlsY + button * 0.35f, button)
        addCircle("A", GbaKeyMask.A, faceX, controlsY - button * 0.45f, button)
        addRoundRect("SELECT", GbaKeyMask.Select, width / 2f - button * 1.75f, controlsY + button * 1.05f, button * 1.55f, button * 0.58f)
        addRoundRect("START", GbaKeyMask.Start, width / 2f + button * 0.2f, controlsY + button * 1.05f, button * 1.55f, button * 0.58f)
        addRoundRect("L", GbaKeyMask.L, padding, padding, button * 1.7f, button * 0.7f)
        addRoundRect("R", GbaKeyMask.R, width - padding - button * 1.7f, padding, button * 1.7f, button * 0.7f)
    }

    private fun addCircle(label: String, mask: Int, centerX: Float, centerY: Float, size: Float) {
        val radius = size / 2f
        regions += Region(
            label = label,
            mask = mask,
            bounds = RectF(centerX - radius, centerY - radius, centerX + radius, centerY + radius),
            shape = Shape.Circle,
        )
    }

    private fun addRoundRect(label: String, mask: Int, left: Float, top: Float, width: Float, height: Float) {
        regions += Region(
            label = label,
            mask = mask,
            bounds = RectF(left, top, left + width, top + height),
            shape = Shape.RoundRect,
        )
    }

    private fun keysAt(x: Float, y: Float): Int {
        var keys = 0
        for (region in regions) {
            if (contains(region, x, y)) {
                keys = keys or region.mask
            }
        }
        return keys
    }

    private fun contains(region: Region, x: Float, y: Float): Boolean {
        return when (region.shape) {
            Shape.Circle -> {
                val radius = region.bounds.width() / 2f
                val dx = x - region.bounds.centerX()
                val dy = y - region.bounds.centerY()
                dx * dx + dy * dy <= radius * radius
            }
            Shape.RoundRect -> region.bounds.contains(x, y)
        }
    }

    private fun drawRegion(canvas: Canvas, region: Region, paint: Paint) {
        when (region.shape) {
            Shape.Circle -> canvas.drawOval(region.bounds, paint)
            Shape.RoundRect -> canvas.drawRoundRect(region.bounds, dp(14f), dp(14f), paint)
        }
    }

    private fun updateKeys(keys: Int) {
        if (pressedKeys == keys) {
            return
        }
        if (keys and pressedKeys.inv() != 0) {
            performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
        }
        pressedKeys = keys
        onKeysChanged?.invoke(keys)
        invalidate()
    }

    private fun dp(value: Float): Float {
        return value * resources.displayMetrics.density
    }

}
