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
import kotlin.math.roundToInt

data class VirtualGamepadLayoutOffsets(
    val dpadXPercent: Int = 0,
    val dpadYPercent: Int = 0,
    val faceXPercent: Int = 0,
    val faceYPercent: Int = 0,
    val centerXPercent: Int = 0,
    val centerYPercent: Int = 0,
    val leftShoulderXPercent: Int = 0,
    val leftShoulderYPercent: Int = 0,
    val rightShoulderXPercent: Int = 0,
    val rightShoulderYPercent: Int = 0,
) {
    fun serialize(): String {
        return listOf(
            dpadXPercent,
            dpadYPercent,
            faceXPercent,
            faceYPercent,
            centerXPercent,
            centerYPercent,
            leftShoulderXPercent,
            leftShoulderYPercent,
            rightShoulderXPercent,
            rightShoulderYPercent,
        ).joinToString(",")
    }

    companion object {
        fun parse(value: String): VirtualGamepadLayoutOffsets {
            val values = value.split(',').map { it.trim().toIntOrNull()?.coerceIn(MinOffset, MaxOffset) ?: 0 }
            return VirtualGamepadLayoutOffsets(
                dpadXPercent = values.getOrElse(0) { 0 },
                dpadYPercent = values.getOrElse(1) { 0 },
                faceXPercent = values.getOrElse(2) { 0 },
                faceYPercent = values.getOrElse(3) { 0 },
                centerXPercent = values.getOrElse(4) { 0 },
                centerYPercent = values.getOrElse(5) { 0 },
                leftShoulderXPercent = values.getOrElse(6) { 0 },
                leftShoulderYPercent = values.getOrElse(7) { 0 },
                rightShoulderXPercent = values.getOrElse(8) { 0 },
                rightShoulderYPercent = values.getOrElse(9) { 0 },
            )
        }

        const val MinOffset = -35
        const val MaxOffset = 35
    }
}

class VirtualGamepadView(context: Context) : View(context) {
    private enum class Cluster {
        Dpad,
        Face,
        Center,
        LeftShoulder,
        RightShoulder,
    }

    private enum class Shape {
        Circle,
        RoundRect,
    }

    private data class Region(
        val label: String,
        val mask: Int,
        val bounds: RectF,
        val shape: Shape,
        val cluster: Cluster,
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
    private var spacingPercent = 100
    private var hapticsEnabled = true
    private var leftHanded = false
    private var layoutOffsets = VirtualGamepadLayoutOffsets()
    private var layoutEditMode = false
    private var activeEditCluster: Cluster? = null
    private var lastEditX = 0f
    private var lastEditY = 0f
    private var onLayoutOffsetsChanged: ((VirtualGamepadLayoutOffsets) -> Unit)? = null

    init {
        isFocusable = true
        isClickable = true
        setWillNotDraw(false)
    }

    fun setOnKeysChangedListener(listener: (Int) -> Unit) {
        onKeysChanged = listener
    }

    fun setOnLayoutOffsetsChangedListener(listener: (VirtualGamepadLayoutOffsets) -> Unit) {
        onLayoutOffsetsChanged = listener
    }

    fun clearKeys() {
        updateKeys(0)
    }

    fun setLayoutOffsets(offsets: VirtualGamepadLayoutOffsets) {
        if (layoutOffsets == offsets) {
            return
        }
        layoutOffsets = offsets
        rebuildRegions(width, height)
        invalidate()
    }

    fun setLayoutEditMode(enabled: Boolean) {
        if (layoutEditMode == enabled) {
            return
        }
        layoutEditMode = enabled
        activeEditCluster = null
        updateKeys(0)
        invalidate()
    }

    fun setStyle(
        sizePercent: Int,
        opacityPercent: Int,
        spacingPercent: Int,
        hapticsEnabled: Boolean,
        leftHanded: Boolean,
    ) {
        val newSizePercent = sizePercent.coerceIn(60, 140)
        val newOpacityPercent = opacityPercent.coerceIn(35, 100)
        val newSpacingPercent = spacingPercent.coerceIn(70, 140)
        if (this.sizePercent == newSizePercent &&
            this.opacityPercent == newOpacityPercent &&
            this.spacingPercent == newSpacingPercent &&
            this.hapticsEnabled == hapticsEnabled &&
            this.leftHanded == leftHanded
        ) {
            return
        }
        this.sizePercent = newSizePercent
        this.opacityPercent = newOpacityPercent
        this.spacingPercent = newSpacingPercent
        this.hapticsEnabled = hapticsEnabled
        this.leftHanded = leftHanded
        rebuildRegions(width, height)
        invalidate()
    }

    override fun onSizeChanged(width: Int, height: Int, oldWidth: Int, oldHeight: Int) {
        rebuildRegions(width, height)
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (layoutEditMode) {
            return handleLayoutEditTouch(event)
        }
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

    private fun handleLayoutEditTouch(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                activeEditCluster = regionAt(event.x, event.y)?.cluster
                lastEditX = event.x
                lastEditY = event.y
                return true
            }
            MotionEvent.ACTION_MOVE -> {
                val cluster = activeEditCluster ?: return true
                val dxPercent = ((event.x - lastEditX) / width.coerceAtLeast(1).toFloat() * 100f).roundToInt()
                val dyPercent = ((event.y - lastEditY) / height.coerceAtLeast(1).toFloat() * 100f).roundToInt()
                if (dxPercent != 0 || dyPercent != 0) {
                    layoutOffsets = layoutOffsets.moved(cluster, dxPercent, dyPercent)
                    lastEditX = event.x
                    lastEditY = event.y
                    rebuildRegions(width, height)
                    onLayoutOffsetsChanged?.invoke(layoutOffsets)
                    invalidate()
                }
                return true
            }
            MotionEvent.ACTION_UP,
            MotionEvent.ACTION_CANCEL -> {
                activeEditCluster = null
                performClick()
                return true
            }
        }
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
            strokePaint.color = if (layoutEditMode) {
                Color.argb((230 * opacity).toInt().coerceIn(80, 255), 255, 214, 102)
            } else {
                Color.argb((170 * opacity).toInt().coerceIn(40, 255), 255, 255, 255)
            }
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
        val spacing = spacingPercent / 100f
        val button = base.times(0.108f).times(scale).coerceIn(dp(36f), dp(104f))
        val smallButton = button * 0.86f
        val padding = dp(24f) * scale
        if (height > width) {
            rebuildPortraitRegions(width, height, padding, button, smallButton, spacing)
        } else {
            rebuildLandscapeRegions(width, height, padding, button, smallButton, spacing)
        }
    }

    private fun rebuildLandscapeRegions(
        width: Int,
        height: Int,
        padding: Float,
        button: Float,
        smallButton: Float,
        spacing: Float,
    ) {
        val controlsY = height - padding - button * (1.25f + 0.4f * spacing)
        val leftClusterX = padding + button * (1.2f + 0.55f * spacing)
        val rightClusterX = width - padding - button * (1.1f + 0.55f * spacing)
        val dpadX = if (leftHanded) rightClusterX else leftClusterX
        val faceX = if (leftHanded) leftClusterX else rightClusterX
        val dpadGap = button * spacing
        val faceGap = button * (0.85f + 0.2f * spacing)
        val centerGap = button * (1.45f + 0.3f * spacing)
        val dpad = offsetFor(Cluster.Dpad, width, height)
        val face = offsetFor(Cluster.Face, width, height)
        val center = offsetFor(Cluster.Center, width, height)
        val leftShoulder = offsetFor(Cluster.LeftShoulder, width, height)
        val rightShoulder = offsetFor(Cluster.RightShoulder, width, height)

        addCircle("UP", GbaKeyMask.Up, dpadX + dpad.first, controlsY - dpadGap + dpad.second, smallButton, Cluster.Dpad)
        addCircle("DOWN", GbaKeyMask.Down, dpadX + dpad.first, controlsY + dpadGap + dpad.second, smallButton, Cluster.Dpad)
        addCircle("LEFT", GbaKeyMask.Left, dpadX - dpadGap + dpad.first, controlsY + dpad.second, smallButton, Cluster.Dpad)
        addCircle("RIGHT", GbaKeyMask.Right, dpadX + dpadGap + dpad.first, controlsY + dpad.second, smallButton, Cluster.Dpad)

        addCircle("B", GbaKeyMask.B, faceX - faceGap + face.first, controlsY + button * 0.35f * spacing + face.second, button, Cluster.Face)
        addCircle("A", GbaKeyMask.A, faceX + face.first, controlsY - button * 0.45f * spacing + face.second, button, Cluster.Face)
        addRoundRect("SELECT", GbaKeyMask.Select, width / 2f - centerGap + center.first, controlsY + button * 1.05f * spacing + center.second, button * 1.55f, button * 0.58f, Cluster.Center)
        addRoundRect("START", GbaKeyMask.Start, width / 2f + button * 0.2f * spacing + center.first, controlsY + button * 1.05f * spacing + center.second, button * 1.55f, button * 0.58f, Cluster.Center)
        addRoundRect("L", GbaKeyMask.L, padding + leftShoulder.first, padding + leftShoulder.second, button * 1.7f, button * 0.7f, Cluster.LeftShoulder)
        addRoundRect("R", GbaKeyMask.R, width - padding - button * 1.7f + rightShoulder.first, padding + rightShoulder.second, button * 1.7f, button * 0.7f, Cluster.RightShoulder)
    }

    private fun rebuildPortraitRegions(
        width: Int,
        height: Int,
        padding: Float,
        button: Float,
        smallButton: Float,
        spacing: Float,
    ) {
        val controlsY = height - padding - button * (2.0f + 0.25f * spacing)
        val leftClusterX = padding + button * (1.2f + 0.45f * spacing)
        val rightClusterX = width - padding - button * (1.0f + 0.45f * spacing)
        val dpadX = if (leftHanded) rightClusterX else leftClusterX
        val faceX = if (leftHanded) leftClusterX else rightClusterX
        val dpadGap = button * spacing
        val faceGap = button * (0.75f + 0.18f * spacing)
        val centerY = height - padding - button * 0.72f
        val shoulderTop = controlsY - button * (1.55f + 0.2f * spacing)
        val dpad = offsetFor(Cluster.Dpad, width, height)
        val face = offsetFor(Cluster.Face, width, height)
        val center = offsetFor(Cluster.Center, width, height)
        val leftShoulder = offsetFor(Cluster.LeftShoulder, width, height)
        val rightShoulder = offsetFor(Cluster.RightShoulder, width, height)

        addCircle("UP", GbaKeyMask.Up, dpadX + dpad.first, controlsY - dpadGap + dpad.second, smallButton, Cluster.Dpad)
        addCircle("DOWN", GbaKeyMask.Down, dpadX + dpad.first, controlsY + dpadGap + dpad.second, smallButton, Cluster.Dpad)
        addCircle("LEFT", GbaKeyMask.Left, dpadX - dpadGap + dpad.first, controlsY + dpad.second, smallButton, Cluster.Dpad)
        addCircle("RIGHT", GbaKeyMask.Right, dpadX + dpadGap + dpad.first, controlsY + dpad.second, smallButton, Cluster.Dpad)

        addCircle("B", GbaKeyMask.B, faceX - faceGap + face.first, controlsY + button * 0.3f * spacing + face.second, button, Cluster.Face)
        addCircle("A", GbaKeyMask.A, faceX + face.first, controlsY - button * 0.42f * spacing + face.second, button, Cluster.Face)
        addRoundRect("SELECT", GbaKeyMask.Select, width / 2f - button * 1.75f + center.first, centerY + center.second, button * 1.55f, button * 0.58f, Cluster.Center)
        addRoundRect("START", GbaKeyMask.Start, width / 2f + button * 0.2f + center.first, centerY + center.second, button * 1.55f, button * 0.58f, Cluster.Center)
        addRoundRect("L", GbaKeyMask.L, padding + leftShoulder.first, shoulderTop + leftShoulder.second, button * 1.7f, button * 0.7f, Cluster.LeftShoulder)
        addRoundRect("R", GbaKeyMask.R, width - padding - button * 1.7f + rightShoulder.first, shoulderTop + rightShoulder.second, button * 1.7f, button * 0.7f, Cluster.RightShoulder)
    }

    private fun addCircle(label: String, mask: Int, centerX: Float, centerY: Float, size: Float, cluster: Cluster) {
        val radius = size / 2f
        regions += Region(
            label = label,
            mask = mask,
            bounds = RectF(centerX - radius, centerY - radius, centerX + radius, centerY + radius),
            shape = Shape.Circle,
            cluster = cluster,
        )
    }

    private fun addRoundRect(label: String, mask: Int, left: Float, top: Float, width: Float, height: Float, cluster: Cluster) {
        regions += Region(
            label = label,
            mask = mask,
            bounds = RectF(left, top, left + width, top + height),
            shape = Shape.RoundRect,
            cluster = cluster,
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

    private fun regionAt(x: Float, y: Float): Region? {
        return regions.lastOrNull { contains(it, x, y) }
    }

    private fun offsetFor(cluster: Cluster, width: Int, height: Int): Pair<Float, Float> {
        val xPercent: Int
        val yPercent: Int
        when (cluster) {
            Cluster.Dpad -> {
                xPercent = layoutOffsets.dpadXPercent
                yPercent = layoutOffsets.dpadYPercent
            }
            Cluster.Face -> {
                xPercent = layoutOffsets.faceXPercent
                yPercent = layoutOffsets.faceYPercent
            }
            Cluster.Center -> {
                xPercent = layoutOffsets.centerXPercent
                yPercent = layoutOffsets.centerYPercent
            }
            Cluster.LeftShoulder -> {
                xPercent = layoutOffsets.leftShoulderXPercent
                yPercent = layoutOffsets.leftShoulderYPercent
            }
            Cluster.RightShoulder -> {
                xPercent = layoutOffsets.rightShoulderXPercent
                yPercent = layoutOffsets.rightShoulderYPercent
            }
        }
        return Pair(width * xPercent / 100f, height * yPercent / 100f)
    }

    private fun VirtualGamepadLayoutOffsets.moved(
        cluster: Cluster,
        dxPercent: Int,
        dyPercent: Int,
    ): VirtualGamepadLayoutOffsets {
        fun next(value: Int, delta: Int): Int {
            return (value + delta).coerceIn(VirtualGamepadLayoutOffsets.MinOffset, VirtualGamepadLayoutOffsets.MaxOffset)
        }
        return when (cluster) {
            Cluster.Dpad -> copy(
                dpadXPercent = next(dpadXPercent, dxPercent),
                dpadYPercent = next(dpadYPercent, dyPercent),
            )
            Cluster.Face -> copy(
                faceXPercent = next(faceXPercent, dxPercent),
                faceYPercent = next(faceYPercent, dyPercent),
            )
            Cluster.Center -> copy(
                centerXPercent = next(centerXPercent, dxPercent),
                centerYPercent = next(centerYPercent, dyPercent),
            )
            Cluster.LeftShoulder -> copy(
                leftShoulderXPercent = next(leftShoulderXPercent, dxPercent),
                leftShoulderYPercent = next(leftShoulderYPercent, dyPercent),
            )
            Cluster.RightShoulder -> copy(
                rightShoulderXPercent = next(rightShoulderXPercent, dxPercent),
                rightShoulderYPercent = next(rightShoulderYPercent, dyPercent),
            )
        }
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
        if (hapticsEnabled && keys and pressedKeys.inv() != 0) {
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
