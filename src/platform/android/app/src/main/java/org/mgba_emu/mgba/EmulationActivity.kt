/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

package org.mgba_emu.mgba

import android.content.pm.ActivityInfo
import android.net.Uri
import android.opengl.GLSurfaceView
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.view.View
import android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.mgba_emu.mgba.core.Core
import org.mgba_emu.mgba.databinding.ActivityEmulationBinding
import org.mgba_emu.mgba.input.InputState
import org.mgba_emu.mgba.model.GameModel
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

    private var currentGame: GameModel? = null

    private var emulationThread: EmulationThread? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityEmulationBinding.inflate(layoutInflater)
        setContentView(binding.root)
        enableFullScreenImmersive()

        requestedOrientation = when (GlobalConfig.screenOrientation) {
            0 -> ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
            1 -> ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
            2 -> ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE
            3 -> ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT
            4 -> ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
            5 -> ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT
            else -> ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            currentGame = intent.getParcelableExtra(GameModel.launchId, GameModel::class.java)
        } else {
            @Suppress("DEPRECATION")
            intent.getParcelableExtra<GameModel>(GameModel.launchId)?.let { game: GameModel? ->
                currentGame = game
            }
        }

        currentGame ?: finish()

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

        loadRomFromUri(currentGame!!.uri)
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
        CoroutineScope(Dispatchers.IO).launch {
            try {
                // stop any previously running emulation thread before loading
                stopEmulationThread()
                val ok = Core.loadRom(uri)

                if (ok) {
                    if (!GlobalConfig.skipBios) {
                        if (BiosStore.has(Core.getPlatform())) {
                            val biosBytes = BiosStore.load(Core.getPlatform())
                            if (!Core.loadBios(biosBytes)) {
                                withContext(Dispatchers.Main) {
                                    Toast.makeText(
                                        this@EmulationActivity,
                                        "Imported BIOS was rejected",
                                        Toast.LENGTH_SHORT
                                    )
                                        .show()
                                }
                            }
                        }
                    }

                    val save = SaveDataStore.load(currentGame?.fileName ?: "")
                    val saveOk = Core.loadSaveData(save)
                    if (!saveOk) {
                        // expected when playing for the first time
                        Log.w(LOG_TAG, "could not restore save data")
                    }
                    Core.reset()
                    Log.i(LOG_TAG, "ROM loaded ${Core.gameTitle()}, ${Core.gameCode()}")
                    startEmulationThread()
                } else {
                    Log.w(LOG_TAG, "Core rejected ROM")
                }
            } catch (e: Exception) {
                Log.e(LOG_TAG, "Error reading ROM: ${e.message}")
            }
        }
    }

    private fun persistSaveData() {
        val fileName = currentGame?.fileName ?: return
        val saveBytes = Core.exportSaveData()
        if (saveBytes.isNotEmpty()) {
            SaveDataStore.save(fileName, saveBytes)
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
    }

    companion object {
        private const val LOG_TAG = "EmulationActivity"
        private const val THREAD_JOIN_TIMEOUT_MS = 500L
    }
}