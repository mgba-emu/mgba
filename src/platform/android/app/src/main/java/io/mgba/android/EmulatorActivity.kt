package io.mgba.android

import android.app.Activity
import android.os.Bundle
import android.view.Gravity
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import io.mgba.android.emulator.EmulatorController
import io.mgba.android.emulator.EmulatorSession

class EmulatorActivity : Activity(), SurfaceHolder.Callback {
    private var controller: EmulatorController? = null
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

        val overlay = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setPadding(dp(24), dp(24), dp(24), dp(24))
        }

        overlay.addView(TextView(this).apply {
            text = getString(R.string.emulator_placeholder)
            textSize = 18f
            gravity = Gravity.CENTER
            setTextColor(getColor(R.color.mgba_text_primary))
        })
        overlay.addView(TextView(this).apply {
            text = getString(R.string.emulator_placeholder_detail)
            textSize = 14f
            gravity = Gravity.CENTER
            setTextColor(getColor(R.color.mgba_text_secondary))
            setPadding(0, dp(10), 0, 0)
        })

        root.addView(
            overlay,
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
        controller?.pause()
        super.onPause()
    }

    override fun onDestroy() {
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
        controller?.pause()
        controller?.setSurface(null)
    }

    private fun dp(value: Int): Int {
        return (value * resources.displayMetrics.density).toInt()
    }
}
