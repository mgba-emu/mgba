package org.mgba_emu.mgba

import android.net.Uri
import android.opengl.GLSurfaceView
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.view.View
import android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.net.toUri
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import org.mgba_emu.mgba.core.Core
import org.mgba_emu.mgba.databinding.ActivityEmulationBinding
import org.mgba_emu.mgba.input.InputState
import org.mgba_emu.mgba.renderer.gl.EmulationThread
import org.mgba_emu.mgba.renderer.gl.FrameBuffer
import org.mgba_emu.mgba.renderer.gl.OpenGLRenderer
import org.mgba_emu.mgba.utils.BiosStore
import org.mgba_emu.mgba.utils.GlobalConfig
import org.mgba_emu.mgba.utils.SaveDataStore
import org.mgba_emu.mgba.utils.applySafePadding

class EmulationActivity : AppCompatActivity() {
    private lateinit var binding: ActivityEmulationBinding
    private lateinit var inputState: InputState
    private lateinit var glSurfaceView: GLSurfaceView
    private lateinit var frameBuffer: FrameBuffer

    private var currentGameCode: String? = null

    private var emulationThread: EmulationThread? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityEmulationBinding.inflate(layoutInflater)
        setContentView(binding.root)
        enableFullScreenImmersive()

        binding.fps.visibility = if (GlobalConfig.fpsCounter) View.VISIBLE else View.GONE
        if (GlobalConfig.fpsCounter) binding.fps.applySafePadding()

        inputState = InputState()
        // placeholder size 0 until the first ROM loads and publishes a real frame
        frameBuffer = FrameBuffer(initialPixelCount = 0)

        if (!Core.init()) {
            Toast.makeText(this, "Failed to initialize emulator core", Toast.LENGTH_LONG).show()
            finish()
            return
        }

        setupGlSurface()
        setupTouchControls()

        intent.getStringExtra("gameUri")?.let { uriString: String ->
            loadRomFromUri(uriString.toUri())
        }
    }

    private fun enableFullScreenImmersive() {
        with(window) {
            WindowCompat.setDecorFitsSystemWindows(this, false)
            val insetsController = WindowInsetsControllerCompat(this, decorView)
            insetsController.apply {
                hide(WindowInsetsCompat.Type.systemBars())
                systemBarsBehavior =
                    WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) attributes.layoutInDisplayCutoutMode = LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
        }
    }

    private fun setupGlSurface() {
        glSurfaceView = binding.glSurfaceView
        glSurfaceView.setEGLContextClientVersion(2)
        glSurfaceView.setRenderer(OpenGLRenderer(frameBuffer))
        glSurfaceView.renderMode = GLSurfaceView.RENDERMODE_WHEN_DIRTY

    }

    private fun setupTouchControls() {
        binding.touchControlsView.inputState = inputState
    }

    private fun loadRomFromUri(uri: Uri) {
        try {
            // stop any previously running emulation thread before loading
            stopEmulationThread()

            val ok = Core.loadRom(uri)
            if (ok) {
                currentGameCode = Core.gameCode()
                if (BiosStore.has(Core.getPlatform())) {
                    val biosBytes = BiosStore.load(Core.getPlatform())
                    if (!Core.loadBios(biosBytes)) {
                        Toast.makeText(this, "Imported BIOS was rejected", Toast.LENGTH_SHORT).show()
                    }
                }

                val save = SaveDataStore.load(currentGameCode!!)
                val saveOk = Core.loadSaveData(save)
                if (!saveOk) {
                    // expected when playing for the first time
                    Log.w("EmulationActivity", "could not restore save data")
                }
                Core.reset()
                Log.i("EmulationActivity", "ROM loaded ${Core.gameTitle()}, ${Core.gameCode()}")
                startEmulationThread()
            } else {
                Log.w("EmulationActivity", "Core rejected ROM")
            }
        } catch (e: Exception) {
            Log.e("EmulationActivity", "Error reading ROM: ${e.message}")
        }
    }

    private fun persistSaveData() {
        val code = currentGameCode ?: return
        val saveBytes = Core.exportSaveData()
        if (saveBytes.isNotEmpty()) {
            SaveDataStore.save(code, saveBytes)
        }
    }

    private fun startEmulationThread() {
        val thread = EmulationThread(
            inputState = inputState,
            frameBuffer = frameBuffer,
            onFrameReady = { glSurfaceView.requestRender() },
            onFpsUpdated = { currentFps ->
                if (GlobalConfig.fpsCounter) {
                    runOnUiThread {
                        binding.fps.text = String.format("%.1f FPS", currentFps)
                    }
                }
            }
        )
        emulationThread = thread
        thread.start()
    }

    private fun stopEmulationThread() {
        emulationThread?.let { thread ->
            thread.requestStop()
            thread.join(THREAD_JOIN_TIMEOUT_MS)
        }
        emulationThread = null
    }

    override fun onPause() {
        super.onPause()
        emulationThread?.paused = true
        glSurfaceView.onPause()
        persistSaveData()
    }

    override fun onResume() {
        super.onResume()
        glSurfaceView.onResume()
        emulationThread?.paused = false
    }

    override fun onDestroy() {
        super.onDestroy()
        persistSaveData()
        stopEmulationThread()
        Core.shutdown()
    }

    companion object {
        private const val THREAD_JOIN_TIMEOUT_MS = 500L
    }
}