package io.mgba.android

import android.app.Activity
import android.os.Bundle
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.widget.FrameLayout
import io.mgba.android.emulator.EmulatorController
import io.mgba.android.emulator.EmulatorSession
import io.mgba.android.input.AndroidInputMapper
import io.mgba.android.input.VirtualGamepadView

class EmulatorActivity : Activity(), SurfaceHolder.Callback {
    private var controller: EmulatorController? = null
    private var gamepadView: VirtualGamepadView? = null
    private var virtualKeys = 0
    private var hardwareButtonKeys = 0
    private var hardwareAxisKeys = 0
    private var hasSurface = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        controller = EmulatorSession.current()
        if (controller == null) {
            finish()
            return
        }

        val root = FrameLayout(this).apply {
            setBackgroundColor(getColor(R.color.mgba_background))
        }

        val surface = SurfaceView(this).apply {
            setBackgroundColor(android.graphics.Color.BLACK)
            holder.addCallback(this@EmulatorActivity)
        }
        root.addView(
            surface,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT,
            ),
        )

        gamepadView = VirtualGamepadView(this).apply {
            setOnKeysChangedListener { keys ->
                virtualKeys = keys
                syncKeys()
            }
        }
        root.addView(
            gamepadView,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT,
            ),
        )

        setContentView(root)
    }

    override fun onResume() {
        super.onResume()
        if (hasSurface) {
            controller?.resume()
        }
    }

    override fun onPause() {
        clearInput()
        controller?.pause()
        super.onPause()
    }

    override fun onDestroy() {
        clearInput()
        controller?.setSurface(null)
        super.onDestroy()
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        hasSurface = true
        controller?.setSurface(holder.surface)
        controller?.start()
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        controller?.setSurface(holder.surface)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        hasSurface = false
        clearInput()
        controller?.pause()
        controller?.setSurface(null)
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        val mask = AndroidInputMapper.keyMaskForKeyCode(event.keyCode)
        if (mask == 0) {
            return super.dispatchKeyEvent(event)
        }

        when (event.action) {
            KeyEvent.ACTION_DOWN -> {
                hardwareButtonKeys = hardwareButtonKeys or mask
                syncKeys()
                return true
            }
            KeyEvent.ACTION_UP -> {
                hardwareButtonKeys = hardwareButtonKeys and mask.inv()
                syncKeys()
                return true
            }
        }
        return super.dispatchKeyEvent(event)
    }

    override fun onGenericMotionEvent(event: MotionEvent): Boolean {
        if (event.actionMasked != MotionEvent.ACTION_MOVE) {
            return super.onGenericMotionEvent(event)
        }
        val keys = AndroidInputMapper.motionKeys(event)
        if (keys == 0 && hardwareAxisKeys == 0) {
            return super.onGenericMotionEvent(event)
        }
        hardwareAxisKeys = keys
        syncKeys()
        return true
    }

    private fun clearInput() {
        virtualKeys = 0
        hardwareButtonKeys = 0
        hardwareAxisKeys = 0
        gamepadView?.clearKeys()
        syncKeys()
    }

    private fun syncKeys() {
        controller?.setKeys(virtualKeys or hardwareButtonKeys or hardwareAxisKeys)
    }
}
