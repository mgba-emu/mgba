/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

package org.mgba_emu.mgba

import android.content.DialogInterface
import android.content.pm.ActivityInfo
import android.net.Uri
import android.opengl.GLSurfaceView
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.view.InputDevice
import android.view.KeyEvent
import android.view.View
import android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.PopupMenu
import androidx.core.graphics.Insets
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import androidx.core.view.isVisible
import androidx.core.view.updatePadding
import androidx.drawerlayout.widget.DrawerLayout
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.mgba_emu.mgba.core.Core
import org.mgba_emu.mgba.databinding.ActivityEmulationBinding
import org.mgba_emu.mgba.input.GbaKey
import org.mgba_emu.mgba.input.InputState
import org.mgba_emu.mgba.model.GameModel
import org.mgba_emu.mgba.renderer.gl.EmulationThread
import org.mgba_emu.mgba.renderer.gl.FrameBuffer
import org.mgba_emu.mgba.renderer.gl.OpenGLRenderer
import org.mgba_emu.mgba.settings.SettingsFragment
import org.mgba_emu.mgba.settings.SettingsFragmentArgs
import org.mgba_emu.mgba.settings.model.Settings
import org.mgba_emu.mgba.utils.BiosStore
import org.mgba_emu.mgba.utils.ConfigManager
import org.mgba_emu.mgba.utils.LifecycleUtils.collect
import org.mgba_emu.mgba.utils.SaveDataStore
import org.mgba_emu.mgba.utils.SearchLocationHelper
import org.mgba_emu.mgba.utils.ViewUtils.applySafePadding

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

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            currentGame = intent.getParcelableExtra(GameModel.launchId, GameModel::class.java)
        } else {
            @Suppress("DEPRECATION")
            intent.getParcelableExtra<GameModel>(GameModel.launchId)?.let { game: GameModel? ->
                currentGame = game
            }
        }

        if (currentGame == null) {
            val gameUri: Uri? = intent.data
            if (gameUri != null) {
                currentGame = SearchLocationHelper.getGame(gameUri)
            }

            currentGame ?: return finish()
        }

        ConfigManager.gameFileName = currentGame!!.fileName
        ConfigManager.shouldHandleInGameMenu = true

        requestedOrientation = when (Settings.screenOrientation.value) {
            0 -> ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
            1 -> ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
            2 -> ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE
            3 -> ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT
            4 -> ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
            5 -> ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT
            else -> ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
        }

        binding.drawerLayout.addDrawerListener(object : DrawerLayout.DrawerListener {
            override fun onDrawerSlide(drawerView: View, slideOffset: Float) {}

            override fun onDrawerOpened(drawerView: View) {
                pauseEmulation()
                binding.inGameMenu.requestFocus()
            }

            override fun onDrawerClosed(drawerView: View) {
                resumeEmulation()
            }

            override fun onDrawerStateChanged(newState: Int) {}
        })

        binding.inGameMenu.getHeaderView(0).findViewById<TextView>(R.id.game_title).text = currentGame!!.title ?: currentGame!!.fileName

        binding.inGameMenu.setNavigationItemSelectedListener {
            when (it.itemId) {
                R.id.menu_resume_emulation -> {
                    binding.drawerLayout.close()
                    true
                }

                R.id.menu_settings -> {
                    openSettings()
                    true
                }

                R.id.menu_settings_per_game -> {
                    openSettings(currentGame)
                    true
                }

                R.id.menu_overlay_options -> {
                    showOverlayOptions()
                    true
                }

                R.id.menu_exit -> {
                    finish()
                    true
                }

                else -> true
            }
        }

        binding.fps.isVisible = Settings.fpsCounter.value
        binding.fps.applySafePadding()

        Settings.fpsCounter.flow.distinctUntilChanged().collect(this) {
            binding.fps.isVisible = it
        }

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
        setInsets()
    }

    private fun openSettings(game: GameModel? = null) {
        ConfigManager.shouldHandleInGameMenu = false
        val settingsFragment = SettingsFragment().apply {
            arguments = SettingsFragmentArgs(
                game,
                Settings.MenuTag.SECTION_ROOT
            ).toBundle()
            onDismiss = {
                ConfigManager.shouldHandleInGameMenu = true
            }
        }
        settingsFragment.show(supportFragmentManager, "SettingsFragment")
    }

    private fun showOverlayOptions() {
        val anchor = binding.inGameMenu.findViewById<View>(R.id.menu_overlay_options)
        val popup = PopupMenu(this, anchor)

        popup.menuInflater.inflate(R.menu.menu_overlay_options, popup.menu)

        popup.menu.apply {
            findItem(R.id.menu_toggle_fps).isChecked = Settings.fpsCounter.value
        }

        popup.setOnMenuItemClickListener {
            when (it.itemId) {
                R.id.menu_toggle_fps -> {
                    it.isChecked = !it.isChecked
                    Settings.fpsCounter.value = it.isChecked
                    true
                }
                else -> true
            }
        }

        popup.show()
    }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.inGameMenu
        ) { v: View, windowInsets: WindowInsetsCompat ->
            val cutInsets: Insets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())
            var left = 0
            var right = 0
            if (ViewCompat.getLayoutDirection(v) == ViewCompat.LAYOUT_DIRECTION_LTR) {
                left = cutInsets.left
            } else {
                right = cutInsets.right
            }

            v.updatePadding(left = left, top = cutInsets.top, right = right)
            windowInsets
        }
    }

    private fun keyCodeToGbaKey(keyCode: Int): Int? {
        // TODO: implement mapping support
        when (keyCode) {
            KeyEvent.KEYCODE_DPAD_UP -> return GbaKey.UP
            KeyEvent.KEYCODE_DPAD_DOWN -> return GbaKey.DOWN
            KeyEvent.KEYCODE_DPAD_LEFT -> return GbaKey.LEFT
            KeyEvent.KEYCODE_DPAD_RIGHT -> return GbaKey.RIGHT
            KeyEvent.KEYCODE_BUTTON_A -> return GbaKey.A
            KeyEvent.KEYCODE_BUTTON_B -> return GbaKey.B
            KeyEvent.KEYCODE_BUTTON_L1 -> return GbaKey.L
            KeyEvent.KEYCODE_BUTTON_R1 -> return GbaKey.R
            KeyEvent.KEYCODE_BUTTON_START -> return GbaKey.START
            KeyEvent.KEYCODE_BUTTON_SELECT -> return GbaKey.SELECT

            KeyEvent.KEYCODE_X -> return GbaKey.A
            KeyEvent.KEYCODE_Z -> return GbaKey.B
            KeyEvent.KEYCODE_A -> return GbaKey.L
            KeyEvent.KEYCODE_S -> return GbaKey.R
            KeyEvent.KEYCODE_ENTER -> return GbaKey.START
            KeyEvent.KEYCODE_BACKSLASH -> return GbaKey.SELECT
        }

        return null
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent?): Boolean {
        if (event == null) return super.onKeyDown(keyCode, event)

        val validSources = InputDevice.SOURCE_GAMEPAD or
                InputDevice.SOURCE_JOYSTICK or
                InputDevice.SOURCE_DPAD or
                InputDevice.SOURCE_KEYBOARD

        if ((event.source and validSources) == 0 || event.repeatCount != 0) {
            return super.onKeyDown(keyCode, event)
        }

        val gbaKey = keyCodeToGbaKey(keyCode)
        gbaKey ?: return super.onKeyDown(keyCode, event)
        inputState.press(gbaKey)
        return true
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent?): Boolean {
        if (event == null) return super.onKeyUp(keyCode, event)

        val validSources = InputDevice.SOURCE_GAMEPAD or
                InputDevice.SOURCE_JOYSTICK or
                InputDevice.SOURCE_DPAD or
                InputDevice.SOURCE_KEYBOARD

        if ((event.source and validSources) == 0) {
            return super.onKeyUp(keyCode, event)
        }

        val gbaKey = keyCodeToGbaKey(keyCode)
        gbaKey ?: return super.onKeyUp(keyCode, event)
        inputState.release(gbaKey)
        return true
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
                    if (!Settings.skipBios.value) {
                        if (BiosStore.has(Core.getPlatform())) {
                            val biosFd = BiosStore.load(Core.getPlatform())
                            if (biosFd != null) {
                                if (!Core.loadBios(biosFd)) {
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
                if (Settings.fpsCounter.value) {
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

    private fun pauseEmulation() {
        emulationThread?.paused = true
        glSurfaceView.onPause()
        persistSaveData()
    }

    private fun resumeEmulation() {
        glSurfaceView.onResume()
        emulationThread?.paused = false
    }

    override fun onPause() {
        super.onPause()
        pauseEmulation()
    }

    override fun onResume() {
        super.onResume()
        resumeEmulation()
    }

    override fun onDestroy() {
        super.onDestroy()
        persistSaveData()
        stopEmulationThread()
        Core.shutdown()
        ConfigManager.gameFileName = null
        ConfigManager.shouldHandleInGameMenu = false
    }

    companion object {
        private const val LOG_TAG = "EmulationActivity"
        private const val THREAD_JOIN_TIMEOUT_MS = 500L
    }
}