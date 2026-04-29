package io.mgba.android

import android.app.Activity
import android.app.AlertDialog
import android.content.pm.ActivityInfo
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.ParcelFileDescriptor
import android.os.SystemClock
import android.os.VibrationEffect
import android.os.Vibrator
import android.provider.OpenableColumns
import android.text.InputType
import android.text.format.DateUtils
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.view.WindowInsets
import android.view.WindowInsetsController
import android.view.WindowManager
import android.widget.Button
import android.widget.CheckBox
import android.widget.EditText
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.SeekBar
import android.widget.TextView
import android.widget.Toast
import io.mgba.android.emulator.EmulatorController
import io.mgba.android.emulator.EmulatorSession
import io.mgba.android.input.AndroidInputMapper
import io.mgba.android.input.GbaButtons
import io.mgba.android.input.GbaKeyMask
import io.mgba.android.input.VirtualGamepadView
import io.mgba.android.library.RomLibraryStore
import io.mgba.android.settings.AudioBufferModes
import io.mgba.android.settings.AudioLowPassModes
import io.mgba.android.settings.EmulatorPreferences
import io.mgba.android.settings.FastForwardModes
import io.mgba.android.settings.InputMappingStore
import io.mgba.android.settings.PerGameOverrideStore
import io.mgba.android.settings.RewindSettings
import io.mgba.android.storage.CheatStore
import io.mgba.android.storage.PatchStore
import io.mgba.android.storage.ScreenshotExporter
import io.mgba.android.storage.ScreenshotShareProvider
import io.mgba.android.storage.SaveExporter
import java.io.File
import java.security.MessageDigest
import java.util.Locale
import kotlin.math.max
import kotlin.math.min

class EmulatorActivity : Activity(), SurfaceHolder.Callback, SensorEventListener {
    private var controller: EmulatorController? = null
    private var gamepadView: VirtualGamepadView? = null
    private lateinit var preferences: EmulatorPreferences
    private lateinit var perGameOverrides: PerGameOverrideStore
    private lateinit var inputMappingStore: InputMappingStore
    private lateinit var cheatStore: CheatStore
    private lateinit var patchStore: PatchStore
    private var currentGameId: String? = null
    private var currentStableGameId: String? = null
    private var activeInputDeviceDescriptor: String? = null
    private var activeInputDeviceName: String? = null
    private var lastInputDeviceDescriptor: String? = null
    private var lastInputDeviceName: String? = null
    private var lastInputKeyCode: Int? = null
    private var lastInputKeyAction = "None"
    private var lastMotionSource = 0
    private var lastMotionKeys = 0
    private var lastAxisX = 0f
    private var lastAxisY = 0f
    private var lastHatX = 0f
    private var lastHatY = 0f
    private var lastLeftTrigger = 0f
    private var lastRightTrigger = 0f
    private var virtualKeys = 0
    private var hardwareButtonKeys = 0
    private var hardwareAxisKeys = 0
    private var stateSlot = 1
    private var slotButton: Button? = null
    private var stateThumbnailView: ImageView? = null
    private var pauseButton: Button? = null
    private var fastButton: Button? = null
    private var fastModeButton: Button? = null
    private var fastMultiplierButton: Button? = null
    private var rewindButton: Button? = null
    private var rewindEnabledButton: Button? = null
    private var rewindBufferButton: Button? = null
    private var rewindIntervalButton: Button? = null
    private var autoStateButton: Button? = null
    private var frameSkipButton: Button? = null
    private var muteButton: Button? = null
    private var volumeButton: Button? = null
    private var audioBufferButton: Button? = null
    private var audioLowPassButton: Button? = null
    private var scaleButton: Button? = null
    private var filterButton: Button? = null
    private var interframeBlendButton: Button? = null
    private var orientationButton: Button? = null
    private var skipBiosButton: Button? = null
    private var padButton: Button? = null
    private var padSettingsButton: Button? = null
    private var deadzoneButton: Button? = null
    private var opposingDirectionsButton: Button? = null
    private var rumbleButton: Button? = null
    private var tiltButton: Button? = null
    private var solarButton: Button? = null
    private var cameraButton: Button? = null
    private var statsButton: Button? = null
    private var statsOverlay: TextView? = null
    private var userPaused = false
    private var fastForward = false
    private var fastForwardMode = FastForwardModes.ModeToggle
    private var fastForwardMultiplier = FastForwardModes.MultiplierMax
    private var rewinding = false
    private var rewindEnabled = true
    private var rewindBufferCapacity = 600
    private var rewindBufferInterval = 1
    private var autoStateOnExit = false
    private var frameSkip = 0
    private var muted = false
    private var volumePercent = 100
    private var audioBufferMode = 1
    private var audioLowPassMode = 0
    private var showVirtualGamepad = true
    private var virtualGamepadSizePercent = 100
    private var virtualGamepadOpacityPercent = 100
    private var virtualGamepadSpacingPercent = 100
    private var virtualGamepadHapticsEnabled = true
    private var virtualGamepadLeftHanded = false
    private var deadzonePercent = AndroidInputMapper.DefaultAxisThresholdPercent
    private var allowOpposingDirections = true
    private var rumbleEnabled = true
    private var tiltEnabled = false
    private var lastRawTiltX = 0f
    private var lastRawTiltY = 0f
    private var tiltOffsetX = 0f
    private var tiltOffsetY = 0f
    private var gyroZ = 0f
    private var solarLevel = 255
    private var useLightSensor = false
    private var cameraImagePath = ""
    private var showStats = false
    private var lastStatsFrames = 0L
    private var lastStatsAtMs = 0L
    private var pendingExportStateSlot = 1
    private var pendingImportStateSlot = 1
    private var pendingHardwareMappingMask = 0
    private var playAccountingStartedAtMs = 0L
    private var scaleMode = 0
    private var filterMode = 0
    private var interframeBlending = false
    private var orientationMode = 0
    private var skipBios = false
    private var hasSurface = false
    private var inputMappingDialog: AlertDialog? = null
    private var keyCaptureDialog: AlertDialog? = null
    private val statsHandler = Handler(Looper.getMainLooper())
    private val rumbleHandler = Handler(Looper.getMainLooper())
    private val vibrator: Vibrator? by lazy { getSystemService(Vibrator::class.java) }
    private val sensorManager: SensorManager? by lazy { getSystemService(SensorManager::class.java) }
    private val accelerometer: Sensor? by lazy { sensorManager?.getDefaultSensor(Sensor.TYPE_ACCELEROMETER) }
    private val gyroscope: Sensor? by lazy { sensorManager?.getDefaultSensor(Sensor.TYPE_GYROSCOPE) }
    private val lightSensor: Sensor? by lazy { sensorManager?.getDefaultSensor(Sensor.TYPE_LIGHT) }
    private var lastRumbleAtMs = 0L
    private val statsRunnable = object : Runnable {
        override fun run() {
            updateStatsOverlay()
            if (showStats) {
                statsHandler.postDelayed(this, 1000L)
            }
        }
    }
    private val rumbleRunnable = object : Runnable {
        override fun run() {
            pollRumble()
            rumbleHandler.postDelayed(this, RUMBLE_POLL_MS)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        preferences = EmulatorPreferences(this)
        perGameOverrides = PerGameOverrideStore(this)
        inputMappingStore = InputMappingStore(this)
        cheatStore = CheatStore(this)
        patchStore = PatchStore(this)
        val currentGame = EmulatorSession.currentGame()
        currentGameId = currentGame?.uri
        currentStableGameId = currentGame?.stableId?.takeIf { it.isNotBlank() }
        scaleMode = perGameOverrides.scaleMode(currentGameId, preferences.scaleMode)
        filterMode = perGameOverrides.filterMode(currentGameId, preferences.filterMode)
        interframeBlending = perGameOverrides.interframeBlending(currentGameId, preferences.interframeBlending)
        orientationMode = perGameOverrides.orientationMode(currentGameId, preferences.orientationMode)
        skipBios = perGameOverrides.skipBios(currentGameId, preferences.skipBios)
        frameSkip = perGameOverrides.frameSkip(currentGameId, preferences.frameSkip)
        muted = perGameOverrides.muted(currentGameId, preferences.muted)
        volumePercent = perGameOverrides.volumePercent(currentGameId, preferences.volumePercent)
        audioBufferMode = perGameOverrides.audioBufferMode(currentGameId, preferences.audioBufferMode)
        audioLowPassMode = perGameOverrides.audioLowPassMode(currentGameId, preferences.audioLowPassMode)
        fastForwardMode = perGameOverrides.fastForwardMode(currentGameId, preferences.fastForwardMode)
        fastForwardMultiplier = perGameOverrides.fastForwardMultiplier(currentGameId, preferences.fastForwardMultiplier)
        rewindEnabled = perGameOverrides.rewindEnabled(currentGameId, preferences.rewindEnabled)
        rewindBufferCapacity = perGameOverrides.rewindBufferCapacity(currentGameId, preferences.rewindBufferCapacity)
        rewindBufferInterval = perGameOverrides.rewindBufferInterval(currentGameId, preferences.rewindBufferInterval)
        autoStateOnExit = preferences.autoStateOnExit
        showVirtualGamepad = perGameOverrides.showVirtualGamepad(currentGameId, preferences.showVirtualGamepad)
        virtualGamepadSizePercent = perGameOverrides.virtualGamepadSizePercent(
            currentGameId,
            preferences.virtualGamepadSizePercent,
        )
        virtualGamepadOpacityPercent = perGameOverrides.virtualGamepadOpacityPercent(
            currentGameId,
            preferences.virtualGamepadOpacityPercent,
        )
        virtualGamepadSpacingPercent = perGameOverrides.virtualGamepadSpacingPercent(
            currentGameId,
            preferences.virtualGamepadSpacingPercent,
        )
        virtualGamepadHapticsEnabled = perGameOverrides.virtualGamepadHapticsEnabled(
            currentGameId,
            preferences.virtualGamepadHapticsEnabled,
        )
        virtualGamepadLeftHanded = perGameOverrides.virtualGamepadLeftHanded(
            currentGameId,
            preferences.virtualGamepadLeftHanded,
        )
        deadzonePercent = perGameOverrides.deadzonePercent(
            currentGameId,
            AndroidInputMapper.DefaultAxisThresholdPercent,
        )
        allowOpposingDirections = perGameOverrides.allowOpposingDirections(
            currentGameId,
            preferences.allowOpposingDirections,
        )
        rumbleEnabled = perGameOverrides.rumbleEnabled(
            currentGameId,
            preferences.rumbleEnabled,
        )
        tiltEnabled = perGameOverrides.tiltEnabled(currentGameId, false)
        tiltOffsetX = perGameOverrides.tiltOffsetX(currentGameId, 0f)
        tiltOffsetY = perGameOverrides.tiltOffsetY(currentGameId, 0f)
        solarLevel = perGameOverrides.solarLevel(currentGameId, 255)
        useLightSensor = perGameOverrides.useLightSensor(currentGameId, false)
        cameraImagePath = perGameOverrides.cameraImagePath(currentGameId)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        applyOrientationMode()
        enterImmersiveMode()
        controller = EmulatorSession.current()
        if (controller == null) {
            finish()
            return
        }
        controller?.setScaleMode(scaleMode)
        controller?.setFilterMode(filterMode)
        controller?.setInterframeBlending(interframeBlending)
        controller?.setSkipBios(skipBios)
        controller?.setFrameSkip(frameSkip)
        controller?.setAudioEnabled(!muted)
        controller?.setVolumePercent(volumePercent)
        controller?.setAudioBufferSamples(AudioBufferModes.samplesFor(audioBufferMode))
        controller?.setLowPassRangePercent(AudioLowPassModes.rangeFor(audioLowPassMode))
        controller?.setFastForwardMultiplier(fastForwardMultiplier)
        controller?.setRewindConfig(rewindEnabled, rewindBufferCapacity, rewindBufferInterval)
        applyPersistedCameraImage()

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
            visibility = if (showVirtualGamepad) View.VISIBLE else View.GONE
            setStyle(
                virtualGamepadSizePercent,
                virtualGamepadOpacityPercent,
                virtualGamepadSpacingPercent,
                virtualGamepadHapticsEnabled,
                virtualGamepadLeftHanded,
            )
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
        root.addView(
            createStateToolbar(),
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT,
                android.view.Gravity.TOP or android.view.Gravity.CENTER_HORIZONTAL,
            ).apply {
                topMargin = dp(12)
            },
        )
        statsOverlay = TextView(this).apply {
            visibility = View.GONE
            textSize = 12f
            setTextColor(android.graphics.Color.WHITE)
            setBackgroundColor(0x99000000.toInt())
            setPadding(dp(8), dp(6), dp(8), dp(6))
        }
        root.addView(
            statsOverlay,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT,
                android.view.Gravity.BOTTOM or android.view.Gravity.START,
            ).apply {
                leftMargin = dp(12)
                bottomMargin = dp(12)
            },
        )

        setContentView(root)
    }

    override fun onResume() {
        super.onResume()
        enterImmersiveMode()
        if (hasSurface && !userPaused) {
            controller?.resume()
            startPlayAccounting()
        }
        startRumblePolling()
        updateSensorRegistration()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) {
            enterImmersiveMode()
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (resultCode != RESULT_OK) {
            return
        }
        val uri = data?.data ?: return
        when (requestCode) {
            REQUEST_IMPORT_SAVE -> importBatterySave(uri)
            REQUEST_IMPORT_CHEATS -> importCheats(uri)
            REQUEST_EXPORT_STATE -> exportStateSlot(uri, pendingExportStateSlot)
            REQUEST_IMPORT_STATE -> importStateSlot(uri, pendingImportStateSlot)
            REQUEST_EXPORT_INPUT_PROFILE -> exportInputProfile(uri)
            REQUEST_IMPORT_INPUT_PROFILE -> importInputProfile(uri)
            REQUEST_IMPORT_PATCH -> importPatch(uri)
            REQUEST_IMPORT_CAMERA_IMAGE -> importCameraImage(uri)
        }
    }

    override fun onPause() {
        clearInput()
        recordPlayTime()
        stopRumblePolling()
        unregisterSensors()
        stopStatsOverlay()
        controller?.pause()
        super.onPause()
    }

    override fun onDestroy() {
        clearInput()
        recordPlayTime()
        stopRumblePolling()
        unregisterSensors()
        stopStatsOverlay()
        controller?.setSurface(null)
        if (isFinishing) {
            if (autoStateOnExit) {
                controller?.saveAutoState()
            }
            EmulatorSession.close()
        }
        super.onDestroy()
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        hasSurface = true
        controller?.setSurface(holder.surface)
        controller?.start()
        if (userPaused) {
            controller?.pause()
        } else {
            controller?.resume()
            startRumblePolling()
            startPlayAccounting()
        }
        updateSensorRegistration()
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        controller?.setSurface(holder.surface)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        hasSurface = false
        clearInput()
        recordPlayTime()
        unregisterSensors()
        controller?.pause()
        controller?.setSurface(null)
    }

    override fun onSensorChanged(event: SensorEvent) {
        when (event.sensor.type) {
            Sensor.TYPE_ACCELEROMETER -> {
                if (!tiltEnabled) {
                    return
                }
                lastRawTiltX = clamp(event.values[0] / SensorManager.GRAVITY_EARTH)
                lastRawTiltY = clamp(event.values[1] / SensorManager.GRAVITY_EARTH)
                syncRotation()
            }
            Sensor.TYPE_GYROSCOPE -> {
                if (!tiltEnabled) {
                    return
                }
                gyroZ = clamp(event.values[2] / MAX_GYRO_RADIANS)
                syncRotation()
            }
            Sensor.TYPE_LIGHT -> {
                if (useLightSensor) {
                    solarLevel = luxToSolarLevel(event.values[0])
                    controller?.setSolarLevel(solarLevel)
                }
            }
        }
    }

    override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) = Unit

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        rememberLastKeyEvent(event)
        if (pendingHardwareMappingMask != 0) {
            if (event.action == KeyEvent.ACTION_DOWN) {
                captureHardwareMappingKey(event)
            }
            return true
        }

        val mask = AndroidInputMapper.keyMaskForKeyCode(
            event.keyCode,
            inputMappingStore.profile(currentGameId, event.deviceDescriptor()),
        )
        if (mask == 0) {
            return super.dispatchKeyEvent(event)
        }

        when (event.action) {
            KeyEvent.ACTION_DOWN -> {
                rememberInputDevice(event)
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
        val keys = AndroidInputMapper.motionKeys(event, deadzonePercent)
        rememberLastMotionEvent(event, keys)
        if (isGamepadMotionSource(event.source)) {
            rememberInputDevice(event)
        }
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
        if (fastForwardMode == FastForwardModes.ModeHold) {
            setFastForwardActive(false)
        }
        setRewindingActive(false)
    }

    private fun syncKeys() {
        val keys = virtualKeys or hardwareButtonKeys or hardwareAxisKeys
        controller?.setKeys(if (allowOpposingDirections) keys else filterOpposingDirections(keys))
    }

    private fun saveScaleModePreference() {
        if (!perGameOverrides.setScaleMode(currentGameId, scaleMode)) {
            preferences.scaleMode = scaleMode
        }
    }

    private fun saveFilterModePreference() {
        if (!perGameOverrides.setFilterMode(currentGameId, filterMode)) {
            preferences.filterMode = filterMode
        }
    }

    private fun saveInterframeBlendingPreference() {
        if (!perGameOverrides.setInterframeBlending(currentGameId, interframeBlending)) {
            preferences.interframeBlending = interframeBlending
        }
    }

    private fun saveOrientationModePreference() {
        if (!perGameOverrides.setOrientationMode(currentGameId, orientationMode)) {
            preferences.orientationMode = orientationMode
        }
    }

    private fun saveSkipBiosPreference() {
        if (!perGameOverrides.setSkipBios(currentGameId, skipBios)) {
            preferences.skipBios = skipBios
        }
    }

    private fun saveMutedPreference() {
        if (!perGameOverrides.setMuted(currentGameId, muted)) {
            preferences.muted = muted
        }
    }

    private fun saveVolumePreference() {
        if (!perGameOverrides.setVolumePercent(currentGameId, volumePercent)) {
            preferences.volumePercent = volumePercent
        }
    }

    private fun saveAudioBufferPreference() {
        if (!perGameOverrides.setAudioBufferMode(currentGameId, audioBufferMode)) {
            preferences.audioBufferMode = audioBufferMode
        }
    }

    private fun saveAudioLowPassPreference() {
        if (!perGameOverrides.setAudioLowPassMode(currentGameId, audioLowPassMode)) {
            preferences.audioLowPassMode = audioLowPassMode
        }
    }

    private fun saveFastForwardModePreference() {
        if (!perGameOverrides.setFastForwardMode(currentGameId, fastForwardMode)) {
            preferences.fastForwardMode = fastForwardMode
        }
    }

    private fun saveFastForwardMultiplierPreference() {
        if (!perGameOverrides.setFastForwardMultiplier(currentGameId, fastForwardMultiplier)) {
            preferences.fastForwardMultiplier = fastForwardMultiplier
        }
    }

    private fun saveRewindPreference() {
        if (!perGameOverrides.setRewindEnabled(currentGameId, rewindEnabled)) {
            preferences.rewindEnabled = rewindEnabled
        }
        if (!perGameOverrides.setRewindBufferCapacity(currentGameId, rewindBufferCapacity)) {
            preferences.rewindBufferCapacity = rewindBufferCapacity
        }
        if (!perGameOverrides.setRewindBufferInterval(currentGameId, rewindBufferInterval)) {
            preferences.rewindBufferInterval = rewindBufferInterval
        }
    }

    private fun saveAutoStatePreference() {
        preferences.autoStateOnExit = autoStateOnExit
    }

    private fun saveGamepadPreference() {
        if (!perGameOverrides.setShowVirtualGamepad(currentGameId, showVirtualGamepad)) {
            preferences.showVirtualGamepad = showVirtualGamepad
        }
    }

    private fun saveGamepadStylePreference() {
        if (!perGameOverrides.setVirtualGamepadSizePercent(currentGameId, virtualGamepadSizePercent)) {
            preferences.virtualGamepadSizePercent = virtualGamepadSizePercent
        }
        if (!perGameOverrides.setVirtualGamepadOpacityPercent(currentGameId, virtualGamepadOpacityPercent)) {
            preferences.virtualGamepadOpacityPercent = virtualGamepadOpacityPercent
        }
        if (!perGameOverrides.setVirtualGamepadSpacingPercent(currentGameId, virtualGamepadSpacingPercent)) {
            preferences.virtualGamepadSpacingPercent = virtualGamepadSpacingPercent
        }
        if (!perGameOverrides.setVirtualGamepadHapticsEnabled(currentGameId, virtualGamepadHapticsEnabled)) {
            preferences.virtualGamepadHapticsEnabled = virtualGamepadHapticsEnabled
        }
        if (!perGameOverrides.setVirtualGamepadLeftHanded(currentGameId, virtualGamepadLeftHanded)) {
            preferences.virtualGamepadLeftHanded = virtualGamepadLeftHanded
        }
    }

    private fun saveFrameSkipPreference() {
        if (!perGameOverrides.setFrameSkip(currentGameId, frameSkip)) {
            preferences.frameSkip = frameSkip
        }
    }

    private fun saveDeadzonePreference() {
        perGameOverrides.setDeadzonePercent(currentGameId, deadzonePercent)
    }

    private fun saveOpposingDirectionsPreference() {
        if (!perGameOverrides.setAllowOpposingDirections(currentGameId, allowOpposingDirections)) {
            preferences.allowOpposingDirections = allowOpposingDirections
        }
    }

    private fun saveRumblePreference() {
        if (!perGameOverrides.setRumbleEnabled(currentGameId, rumbleEnabled)) {
            preferences.rumbleEnabled = rumbleEnabled
        }
    }

    private fun saveSensorPreference() {
        perGameOverrides.setTiltEnabled(currentGameId, tiltEnabled)
        perGameOverrides.setTiltCalibration(currentGameId, tiltOffsetX, tiltOffsetY)
        perGameOverrides.setSolarLevel(currentGameId, solarLevel)
        perGameOverrides.setUseLightSensor(currentGameId, useLightSensor)
    }

    private fun startPlayAccounting() {
        if (playAccountingStartedAtMs == 0L) {
            playAccountingStartedAtMs = SystemClock.elapsedRealtime()
        }
    }

    private fun recordPlayTime() {
        val startedAt = playAccountingStartedAtMs
        if (startedAt == 0L) {
            return
        }
        playAccountingStartedAtMs = 0L
        val seconds = ((SystemClock.elapsedRealtime() - startedAt) / 1000L).coerceAtLeast(0L)
        val gameId = currentGameId ?: return
        runCatching {
            RomLibraryStore(this).addPlayTime(Uri.parse(gameId), seconds)
        }
    }

    private fun createStateToolbar(): LinearLayout {
        return LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            alpha = 0.86f
            val runRow = LinearLayout(context).apply {
                orientation = LinearLayout.HORIZONTAL
            }
            val stateRow = LinearLayout(context).apply {
                orientation = LinearLayout.HORIZONTAL
            }
            pauseButton = Button(context).apply {
                setOnClickListener {
                    userPaused = !userPaused
                    if (userPaused) {
                        recordPlayTime()
                        stopRumblePolling()
                        controller?.pause()
                    } else {
                        controller?.resume()
                        startRumblePolling()
                        startPlayAccounting()
                    }
                    updateSensorRegistration()
                    updateRunButtons()
                }
            }
            runRow.addView(pauseButton)
            runRow.addView(Button(context).apply {
                text = "Reset"
                setOnClickListener {
                    controller?.reset()
                    Toast.makeText(context, "Reset", Toast.LENGTH_SHORT).show()
                }
            })
            runRow.addView(Button(context).apply {
                text = "Step"
                setOnClickListener {
                    stepFrame()
                }
            })
            fastButton = Button(context).apply {
                setOnClickListener {
                    if (fastForwardMode == FastForwardModes.ModeToggle) {
                        setFastForwardActive(!fastForward)
                    }
                }
                setOnTouchListener { view, event ->
                    if (fastForwardMode != FastForwardModes.ModeHold) {
                        return@setOnTouchListener false
                    }
                    when (event.actionMasked) {
                        MotionEvent.ACTION_DOWN -> {
                            setFastForwardActive(true)
                            true
                        }
                        MotionEvent.ACTION_UP -> {
                            setFastForwardActive(false)
                            view.performClick()
                            true
                        }
                        MotionEvent.ACTION_CANCEL -> {
                            setFastForwardActive(false)
                            true
                        }
                        else -> true
                    }
                }
            }
            runRow.addView(fastButton)
            fastModeButton = Button(context).apply {
                setOnClickListener {
                    fastForwardMode = if (fastForwardMode == FastForwardModes.ModeToggle) {
                        FastForwardModes.ModeHold
                    } else {
                        FastForwardModes.ModeToggle
                    }
                    if (fastForwardMode == FastForwardModes.ModeHold) {
                        setFastForwardActive(false)
                    }
                    saveFastForwardModePreference()
                    updateRunButtons()
                }
            }
            runRow.addView(fastModeButton)
            fastMultiplierButton = Button(context).apply {
                setOnClickListener {
                    fastForwardMultiplier = FastForwardModes.nextMultiplier(fastForwardMultiplier)
                    controller?.setFastForwardMultiplier(fastForwardMultiplier)
                    saveFastForwardMultiplierPreference()
                    updateRunButtons()
                }
            }
            runRow.addView(fastMultiplierButton)
            rewindButton = Button(context).apply {
                setOnClickListener {
                    if (!rewindEnabled) {
                        Toast.makeText(context, "Rewind disabled", Toast.LENGTH_SHORT).show()
                    }
                }
                setOnTouchListener { view, event ->
                    if (!rewindEnabled) {
                        return@setOnTouchListener false
                    }
                    when (event.actionMasked) {
                        MotionEvent.ACTION_DOWN -> {
                            setRewindingActive(true)
                            true
                        }
                        MotionEvent.ACTION_UP -> {
                            setRewindingActive(false)
                            view.performClick()
                            true
                        }
                        MotionEvent.ACTION_CANCEL -> {
                            setRewindingActive(false)
                            true
                        }
                        else -> true
                    }
                }
            }
            runRow.addView(rewindButton)
            rewindEnabledButton = Button(context).apply {
                setOnClickListener {
                    rewindEnabled = !rewindEnabled
                    setRewindingActive(false)
                    controller?.setRewindConfig(rewindEnabled, rewindBufferCapacity, rewindBufferInterval)
                    saveRewindPreference()
                    updateRunButtons()
                }
            }
            runRow.addView(rewindEnabledButton)
            rewindBufferButton = Button(context).apply {
                setOnClickListener {
                    rewindBufferCapacity = RewindSettings.nextCapacity(rewindBufferCapacity)
                    setRewindingActive(false)
                    controller?.setRewindConfig(rewindEnabled, rewindBufferCapacity, rewindBufferInterval)
                    saveRewindPreference()
                    updateRunButtons()
                }
            }
            runRow.addView(rewindBufferButton)
            rewindIntervalButton = Button(context).apply {
                setOnClickListener {
                    rewindBufferInterval = RewindSettings.nextInterval(rewindBufferInterval)
                    setRewindingActive(false)
                    controller?.setRewindConfig(rewindEnabled, rewindBufferCapacity, rewindBufferInterval)
                    saveRewindPreference()
                    updateRunButtons()
                }
            }
            runRow.addView(rewindIntervalButton)
            autoStateButton = Button(context).apply {
                setOnClickListener {
                    autoStateOnExit = !autoStateOnExit
                    saveAutoStatePreference()
                    updateRunButtons()
                }
            }
            runRow.addView(autoStateButton)
            frameSkipButton = Button(context).apply {
                setOnClickListener {
                    frameSkip = (frameSkip + 1) % FRAME_SKIP_LABELS.size
                    controller?.setFrameSkip(frameSkip)
                    saveFrameSkipPreference()
                    updateRunButtons()
                }
            }
            runRow.addView(frameSkipButton)
            muteButton = Button(context).apply {
                setOnClickListener {
                    muted = !muted
                    controller?.setAudioEnabled(!muted)
                    saveMutedPreference()
                    updateRunButtons()
                }
            }
            runRow.addView(muteButton)
            volumeButton = Button(context).apply {
                setOnClickListener {
                    val index = VOLUME_LEVELS.indexOf(volumePercent).takeIf { it >= 0 } ?: 0
                    volumePercent = VOLUME_LEVELS[(index + 1) % VOLUME_LEVELS.size]
                    controller?.setVolumePercent(volumePercent)
                    saveVolumePreference()
                    updateRunButtons()
                }
            }
            runRow.addView(volumeButton)
            audioBufferButton = Button(context).apply {
                setOnClickListener {
                    audioBufferMode = (audioBufferMode + 1) % AudioBufferModes.labels.size
                    controller?.setAudioBufferSamples(AudioBufferModes.samplesFor(audioBufferMode))
                    saveAudioBufferPreference()
                    updateRunButtons()
                }
            }
            runRow.addView(audioBufferButton)
            audioLowPassButton = Button(context).apply {
                setOnClickListener {
                    audioLowPassMode = (audioLowPassMode + 1) % AudioLowPassModes.labels.size
                    controller?.setLowPassRangePercent(AudioLowPassModes.rangeFor(audioLowPassMode))
                    saveAudioLowPassPreference()
                    updateRunButtons()
                }
            }
            runRow.addView(audioLowPassButton)
            scaleButton = Button(context).apply {
                setOnClickListener {
                    scaleMode = (scaleMode + 1) % SCALE_LABELS.size
                    controller?.setScaleMode(scaleMode)
                    saveScaleModePreference()
                    updateRunButtons()
                }
            }
            runRow.addView(scaleButton)
            filterButton = Button(context).apply {
                setOnClickListener {
                    filterMode = (filterMode + 1) % FILTER_LABELS.size
                    controller?.setFilterMode(filterMode)
                    saveFilterModePreference()
                    updateRunButtons()
                }
            }
            runRow.addView(filterButton)
            interframeBlendButton = Button(context).apply {
                setOnClickListener {
                    interframeBlending = !interframeBlending
                    controller?.setInterframeBlending(interframeBlending)
                    saveInterframeBlendingPreference()
                    updateRunButtons()
                }
            }
            runRow.addView(interframeBlendButton)
            orientationButton = Button(context).apply {
                setOnClickListener {
                    orientationMode = (orientationMode + 1) % ORIENTATION_LABELS.size
                    applyOrientationMode()
                    saveOrientationModePreference()
                    updateRunButtons()
                }
            }
            runRow.addView(orientationButton)
            skipBiosButton = Button(context).apply {
                setOnClickListener {
                    skipBios = !skipBios
                    controller?.setSkipBios(skipBios)
                    saveSkipBiosPreference()
                    updateRunButtons()
                    Toast.makeText(context, "BIOS setting applies on reset or next launch", Toast.LENGTH_SHORT).show()
                }
            }
            runRow.addView(skipBiosButton)
            padButton = Button(context).apply {
                setOnClickListener {
                    showVirtualGamepad = !showVirtualGamepad
                    saveGamepadPreference()
                    gamepadView?.visibility = if (showVirtualGamepad) View.VISIBLE else View.GONE
                    if (!showVirtualGamepad) {
                        gamepadView?.clearKeys()
                    }
                    updateRunButtons()
                }
            }
            runRow.addView(padButton)
            padSettingsButton = Button(context).apply {
                setOnClickListener {
                    showGamepadSettingsDialog()
                }
            }
            runRow.addView(padSettingsButton)
            deadzoneButton = Button(context).apply {
                setOnClickListener {
                    val index = DEADZONE_LEVELS.indexOf(deadzonePercent).takeIf { it >= 0 } ?: 2
                    deadzonePercent = DEADZONE_LEVELS[(index + 1) % DEADZONE_LEVELS.size]
                    saveDeadzonePreference()
                    hardwareAxisKeys = 0
                    syncKeys()
                    updateRunButtons()
                }
            }
            runRow.addView(deadzoneButton)
            opposingDirectionsButton = Button(context).apply {
                setOnClickListener {
                    allowOpposingDirections = !allowOpposingDirections
                    saveOpposingDirectionsPreference()
                    syncKeys()
                    updateRunButtons()
                }
            }
            runRow.addView(opposingDirectionsButton)
            rumbleButton = Button(context).apply {
                setOnClickListener {
                    rumbleEnabled = !rumbleEnabled
                    saveRumblePreference()
                    if (!rumbleEnabled) {
                        vibrator?.cancel()
                        lastRumbleAtMs = 0L
                    }
                    updateRunButtons()
                }
            }
            runRow.addView(rumbleButton)
            tiltButton = Button(context).apply {
                setOnClickListener {
                    tiltEnabled = !tiltEnabled
                    saveSensorPreference()
                    updateSensorRegistration()
                    updateRunButtons()
                    Toast.makeText(context, if (tiltEnabled) "Tilt enabled" else "Tilt disabled", Toast.LENGTH_SHORT).show()
                }
            }
            runRow.addView(tiltButton)
            runRow.addView(Button(context).apply {
                text = "Cal"
                setOnClickListener {
                    calibrateTilt()
                }
            })
            solarButton = Button(context).apply {
                setOnClickListener {
                    showSolarDialog()
                }
            }
            runRow.addView(solarButton)
            cameraButton = Button(context).apply {
                setOnClickListener {
                    showCameraImageDialog()
                }
            }
            runRow.addView(cameraButton)
            runRow.addView(Button(context).apply {
                text = "Keys"
                setOnClickListener {
                    showInputMappingDialog()
                }
            })
            statsButton = Button(context).apply {
                setOnClickListener {
                    showStats = !showStats
                    if (showStats) {
                        startStatsOverlay()
                    } else {
                        stopStatsOverlay()
                    }
                    updateRunButtons()
                }
            }
            runRow.addView(statsButton)
            runRow.addView(Button(context).apply {
                text = "Shot"
                setOnClickListener {
                    val path = controller?.takeScreenshot()
                    if (path == null) {
                        Toast.makeText(context, "Screenshot failed", Toast.LENGTH_SHORT).show()
                    } else {
                        recordScreenshotCover(path)
                        shareScreenshot(path)
                    }
                }
            })
            runRow.addView(Button(context).apply {
                text = "Export"
                setOnClickListener {
                    val uri = ScreenshotExporter.exportToPictures(context, screenshotExportName()) { descriptor ->
                        controller?.takeScreenshotFd(descriptor.fd) == true
                    }
                    Toast.makeText(
                        context,
                        if (uri != null) "Screenshot exported" else "Export unavailable",
                        Toast.LENGTH_SHORT,
                    ).show()
                }
            })
            stateRow.addView(Button(context).apply {
                text = "Slots"
                setOnClickListener {
                    showStateSlotsDialog()
                }
            })
            stateRow.addView(Button(context).apply {
                text = "-"
                setOnClickListener {
                    stateSlot = if (stateSlot == 1) 9 else stateSlot - 1
                    updateSlotButton()
                }
            })
            slotButton = Button(context).apply {
                isEnabled = false
            }
            stateRow.addView(slotButton)
            stateRow.addView(Button(context).apply {
                text = "+"
                setOnClickListener {
                    stateSlot = if (stateSlot == 9) 1 else stateSlot + 1
                    updateSlotButton()
                }
            })
            stateRow.addView(Button(context).apply {
                text = "Save"
                setOnClickListener {
                    saveStateWithConfirmation()
                }
            })
            stateRow.addView(Button(context).apply {
                text = "Load"
                setOnClickListener {
                    val ok = controller?.loadStateSlot(stateSlot) == true
                    Toast.makeText(context, if (ok) "State loaded" else "Load failed", Toast.LENGTH_SHORT).show()
                }
            })
            stateRow.addView(Button(context).apply {
                text = "Del"
                setOnClickListener {
                    deleteStateWithConfirmation()
                }
            })
            stateRow.addView(Button(context).apply {
                text = "StateOut"
                setOnClickListener {
                    openStateExportPicker()
                }
            })
            stateRow.addView(Button(context).apply {
                text = "StateIn"
                setOnClickListener {
                    importStateWithConfirmation()
                }
            })
            stateRow.addView(Button(context).apply {
                text = "Backup"
                setOnClickListener {
                    val path = controller?.exportBatterySave()
                    val uri = path?.let { SaveExporter.exportToDocuments(context, it) }
                    Toast.makeText(
                        context,
                        if (uri != null) "Save exported" else "Export failed",
                        Toast.LENGTH_SHORT,
                    ).show()
                }
            })
            stateRow.addView(Button(context).apply {
                text = "Import"
                setOnClickListener {
                    openSaveImportPicker()
                }
            })
            stateRow.addView(Button(context).apply {
                text = "Cheats"
                setOnClickListener {
                    showCheatActionsDialog()
                }
            })
            stateRow.addView(Button(context).apply {
                text = "NoCheat"
                setOnClickListener {
                    clearCheatsWithConfirmation()
                }
            })
            stateRow.addView(Button(context).apply {
                text = "Patch"
                setOnClickListener {
                    openPatchImportPicker()
                }
            })
            stateRow.addView(Button(context).apply {
                text = "NoPatch"
                setOnClickListener {
                    clearPatchWithConfirmation()
                }
            })
            stateRow.addView(Button(context).apply {
                text = "Exit"
                setOnClickListener {
                    exitWithConfirmation()
                }
            })
            addView(runRow)
            addView(stateRow)
            stateThumbnailView = ImageView(context).apply {
                visibility = View.GONE
                scaleType = ImageView.ScaleType.CENTER_CROP
                setBackgroundColor(0x99000000.toInt())
            }
            addView(
                stateThumbnailView,
                LinearLayout.LayoutParams(dp(120), dp(80)).apply {
                    gravity = android.view.Gravity.CENTER_HORIZONTAL
                    topMargin = dp(4)
                },
            )
            updateSlotButton()
            updateRunButtons()
        }
    }

    private fun updateSlotButton() {
        slotButton?.text = "Slot $stateSlot"
        updateStateThumbnail()
    }

    private fun updateStateThumbnail() {
        val file = stateThumbnailFile(stateSlot)
        if (file == null || !file.isFile) {
            stateThumbnailView?.setImageDrawable(null)
            stateThumbnailView?.visibility = View.GONE
            return
        }
        val bitmap = BitmapFactory.decodeFile(file.absolutePath)
        if (bitmap == null) {
            stateThumbnailView?.setImageDrawable(null)
            stateThumbnailView?.visibility = View.GONE
        } else {
            stateThumbnailView?.setImageBitmap(bitmap)
            stateThumbnailView?.visibility = View.VISIBLE
        }
    }

    private fun showStateSlotsDialog() {
        val labels = (1..9).map { slot -> stateSlotLabel(slot) }.toTypedArray()
        AlertDialog.Builder(this)
            .setTitle("Save States")
            .setItems(labels) { _, which ->
                stateSlot = which + 1
                updateSlotButton()
                showStateSlotActionsDialog(stateSlot)
            }
            .setNegativeButton("Close", null)
            .show()
    }

    private fun showStateSlotActionsDialog(slot: Int) {
        val hasState = controller?.hasStateSlot(slot) == true
        val actions = if (hasState) {
            arrayOf("Save", "Load", "Delete", "Export", "Import")
        } else {
            arrayOf("Save", "Import")
        }
        AlertDialog.Builder(this)
            .setTitle("Slot $slot")
            .setItems(actions) { _, which ->
                stateSlot = slot
                updateSlotButton()
                when (actions[which]) {
                    "Save" -> saveStateWithConfirmation()
                    "Load" -> {
                        val ok = controller?.loadStateSlot(stateSlot) == true
                        Toast.makeText(this, if (ok) "State loaded" else "Load failed", Toast.LENGTH_SHORT).show()
                    }
                    "Delete" -> deleteStateWithConfirmation()
                    "Export" -> openStateExportPicker()
                    "Import" -> importStateWithConfirmation()
                }
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun stateSlotLabel(slot: Int): String {
        val modifiedMs = controller?.stateSlotModifiedMs(slot) ?: 0L
        if (modifiedMs <= 0L) {
            return "Slot $slot - Empty"
        }
        val modified = DateUtils.formatDateTime(
            this,
            modifiedMs,
            DateUtils.FORMAT_SHOW_DATE or DateUtils.FORMAT_SHOW_TIME,
        )
        val thumbnail = if (stateThumbnailFile(slot)?.isFile == true) " + thumbnail" else ""
        return "Slot $slot - $modified$thumbnail"
    }

    private fun updateRunButtons() {
        pauseButton?.text = if (userPaused) "Resume" else "Pause"
        fastButton?.text = when {
            fastForward -> "1x"
            fastForwardMode == FastForwardModes.ModeHold -> "Hold"
            else -> "Fast"
        }
        fastModeButton?.text = if (fastForwardMode == FastForwardModes.ModeHold) "Mode:Hold" else "Mode:Tog"
        fastMultiplierButton?.text = "Fwd:${FastForwardModes.labelForMultiplier(fastForwardMultiplier)}"
        rewindButton?.text = if (rewinding) "Rw*" else "Rw"
        rewindEnabledButton?.text = if (rewindEnabled) "RwOn" else "RwOff"
        rewindBufferButton?.text = "RwB$rewindBufferCapacity"
        rewindIntervalButton?.text = "RwI$rewindBufferInterval"
        autoStateButton?.text = if (autoStateOnExit) "AutoSt" else "NoAuto"
        frameSkipButton?.text = FRAME_SKIP_LABELS[frameSkip]
        muteButton?.text = if (muted) "Sound" else "Mute"
        volumeButton?.text = "Vol$volumePercent"
        audioBufferButton?.text = AudioBufferModes.labelFor(audioBufferMode)
        audioLowPassButton?.text = AudioLowPassModes.labelFor(audioLowPassMode)
        scaleButton?.text = SCALE_LABELS[scaleMode]
        filterButton?.text = FILTER_LABELS[filterMode]
        interframeBlendButton?.text = if (interframeBlending) "Blend" else "NoBlend"
        orientationButton?.text = ORIENTATION_LABELS[orientationMode]
        skipBiosButton?.text = if (skipBios) "SkipBIOS" else "BIOS"
        padButton?.text = if (showVirtualGamepad) "Pad" else "No Pad"
        padSettingsButton?.text = "PadCfg"
        deadzoneButton?.text = "DZ$deadzonePercent"
        opposingDirectionsButton?.text = if (allowOpposingDirections) "OppOn" else "OppOff"
        rumbleButton?.text = if (rumbleEnabled) "Rumble" else "NoRumble"
        tiltButton?.text = if (tiltEnabled) "Tilt*" else "Tilt"
        solarButton?.text = if (useLightSensor) "Solar*" else "Solar"
        cameraButton?.text = if (cameraImagePath.isBlank()) "Camera" else "Cam*"
        statsButton?.text = if (showStats) "Stats*" else "Stats"
    }

    private fun stepFrame() {
        if (!userPaused) {
            userPaused = true
            recordPlayTime()
            stopRumblePolling()
            controller?.pause()
            updateSensorRegistration()
        }
        val ok = controller?.stepFrame() == true
        updateRunButtons()
        Toast.makeText(this, if (ok) "Stepped" else "Step failed", Toast.LENGTH_SHORT).show()
    }

    private fun setFastForwardActive(enabled: Boolean) {
        fastForward = enabled
        controller?.setFastForward(enabled)
        updateRunButtons()
    }

    private fun setRewindingActive(enabled: Boolean) {
        val active = enabled && rewindEnabled
        rewinding = active
        controller?.setRewinding(active)
        updateRunButtons()
    }

    private fun filterOpposingDirections(keys: Int): Int {
        var filtered = keys
        if ((filtered and GbaKeyMask.Left) != 0 && (filtered and GbaKeyMask.Right) != 0) {
            filtered = filtered and GbaKeyMask.Left.inv() and GbaKeyMask.Right.inv()
        }
        if ((filtered and GbaKeyMask.Up) != 0 && (filtered and GbaKeyMask.Down) != 0) {
            filtered = filtered and GbaKeyMask.Up.inv() and GbaKeyMask.Down.inv()
        }
        return filtered
    }

    private fun updateSensorRegistration() {
        unregisterSensors()
        if (userPaused || !hasSurface) {
            controller?.setRotation(0f, 0f, 0f)
            return
        }
        if (tiltEnabled) {
            accelerometer?.let { sensorManager?.registerListener(this, it, SensorManager.SENSOR_DELAY_GAME) }
            gyroscope?.let { sensorManager?.registerListener(this, it, SensorManager.SENSOR_DELAY_GAME) }
        }
        if (useLightSensor) {
            lightSensor?.let { sensorManager?.registerListener(this, it, SensorManager.SENSOR_DELAY_NORMAL) }
        }
    }

    private fun unregisterSensors() {
        sensorManager?.unregisterListener(this)
        controller?.setRotation(0f, 0f, 0f)
    }

    private fun calibrateTilt() {
        tiltOffsetX = lastRawTiltX
        tiltOffsetY = lastRawTiltY
        gyroZ = 0f
        syncRotation()
        saveSensorPreference()
        Toast.makeText(this, "Tilt calibrated", Toast.LENGTH_SHORT).show()
    }

    private fun syncRotation() {
        val tiltX = clamp(lastRawTiltX - tiltOffsetX)
        val tiltY = clamp(lastRawTiltY - tiltOffsetY)
        controller?.setRotation(tiltX, tiltY, gyroZ)
    }

    private fun clamp(value: Float): Float {
        return max(-1f, min(1f, value))
    }

    private fun showSolarDialog() {
        val label = TextView(this).apply {
            textSize = 16f
            setTextColor(getColor(R.color.mgba_text_primary))
        }
        fun updateLabel() {
            label.text = "Solar level: $solarLevel"
        }
        updateLabel()

        val seekBar = SeekBar(this).apply {
            max = 255
            progress = solarLevel
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                    if (fromUser) {
                        useLightSensor = false
                        solarLevel = progress
                        controller?.setSolarLevel(solarLevel)
                        saveSensorPreference()
                        updateLabel()
                        updateSensorRegistration()
                        updateRunButtons()
                    }
                }

                override fun onStartTrackingTouch(seekBar: SeekBar?) = Unit

                override fun onStopTrackingTouch(seekBar: SeekBar?) = Unit
            })
        }
        val lightCheck = CheckBox(this).apply {
            text = if (lightSensor == null) "Light sensor unavailable" else "Use light sensor"
            isEnabled = lightSensor != null
            isChecked = useLightSensor
            setTextColor(getColor(R.color.mgba_text_primary))
            setOnCheckedChangeListener { _, checked ->
                useLightSensor = checked
                if (!checked) {
                    controller?.setSolarLevel(solarLevel)
                }
                saveSensorPreference()
                updateSensorRegistration()
                updateRunButtons()
            }
        }
        val content = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(16), dp(8), dp(16), dp(4))
            addView(label)
            addView(seekBar)
            addView(lightCheck)
        }
        AlertDialog.Builder(this)
            .setTitle("Solar sensor")
            .setView(content)
            .setPositiveButton("Close", null)
            .show()
    }

    private fun showCameraImageDialog() {
        val actions = if (cameraImagePath.isBlank()) {
            arrayOf("Import Static Image")
        } else {
            arrayOf("Import Static Image", "Clear Static Image")
        }
        AlertDialog.Builder(this)
            .setTitle("Game Boy Camera")
            .setItems(actions) { _, which ->
                when (actions[which]) {
                    "Import Static Image" -> openCameraImagePicker()
                    "Clear Static Image" -> clearCameraImage()
                }
            }
            .show()
    }

    private fun openCameraImagePicker() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "image/*"
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        startActivityForResult(intent, REQUEST_IMPORT_CAMERA_IMAGE)
    }

    private fun importCameraImage(imageUri: Uri) {
        val gameId = currentGameId
        val storageGameId = artifactGameId()
        if (gameId.isNullOrBlank() || storageGameId.isNullOrBlank()) {
            Toast.makeText(this, "Camera image unavailable for this game", Toast.LENGTH_SHORT).show()
            return
        }
        Thread {
            val path = copyCameraImage(storageGameId, imageUri)
            val appliedPath = path?.takeIf { setCameraImageFromPath(it) }
            if (appliedPath != null) {
                perGameOverrides.setCameraImagePath(gameId, appliedPath)
            }
            runOnUiThread {
                if (appliedPath != null) {
                    cameraImagePath = appliedPath
                    updateRunButtons()
                    Toast.makeText(this, "Camera image imported", Toast.LENGTH_SHORT).show()
                } else {
                    Toast.makeText(this, "Camera image import failed", Toast.LENGTH_SHORT).show()
                }
            }
        }.start()
    }

    private fun applyPersistedCameraImage() {
        val path = cameraImagePath
        if (path.isBlank()) {
            return
        }
        if (!setCameraImageFromPath(path)) {
            perGameOverrides.clearCameraImagePath(currentGameId)
            cameraImagePath = ""
        }
    }

    private fun setCameraImageFromPath(path: String): Boolean {
        val image = cameraImagePixels(path) ?: return false
        return controller?.setCameraImage(image.pixels, image.width, image.height) == true
    }

    private fun cameraImagePixels(path: String): CameraImagePixels? {
        val bounds = BitmapFactory.Options().apply {
            inJustDecodeBounds = true
        }
        BitmapFactory.decodeFile(path, bounds)
        if (bounds.outWidth <= 0 || bounds.outHeight <= 0) {
            return null
        }
        val sampleSize = max(
            1,
            min(bounds.outWidth / CAMERA_IMAGE_WIDTH, bounds.outHeight / CAMERA_IMAGE_HEIGHT),
        )
        val source = BitmapFactory.decodeFile(
            path,
            BitmapFactory.Options().apply {
                inSampleSize = sampleSize
            },
        ) ?: return null
        val targetRatio = CAMERA_IMAGE_WIDTH.toFloat() / CAMERA_IMAGE_HEIGHT.toFloat()
        val sourceRatio = source.width.toFloat() / source.height.toFloat()
        val cropWidth: Int
        val cropHeight: Int
        if (sourceRatio > targetRatio) {
            cropHeight = source.height
            cropWidth = (source.height * targetRatio).toInt().coerceIn(1, source.width)
        } else {
            cropWidth = source.width
            cropHeight = (source.width / targetRatio).toInt().coerceIn(1, source.height)
        }
        val cropLeft = ((source.width - cropWidth) / 2).coerceAtLeast(0)
        val cropTop = ((source.height - cropHeight) / 2).coerceAtLeast(0)
        val cropped = Bitmap.createBitmap(source, cropLeft, cropTop, cropWidth, cropHeight)
        val scaled = Bitmap.createScaledBitmap(cropped, CAMERA_IMAGE_WIDTH, CAMERA_IMAGE_HEIGHT, true)
        val pixels = IntArray(CAMERA_IMAGE_WIDTH * CAMERA_IMAGE_HEIGHT)
        scaled.getPixels(pixels, 0, CAMERA_IMAGE_WIDTH, 0, 0, CAMERA_IMAGE_WIDTH, CAMERA_IMAGE_HEIGHT)
        if (scaled !== cropped) {
            scaled.recycle()
        }
        if (cropped !== source) {
            cropped.recycle()
        }
        source.recycle()
        return CameraImagePixels(pixels, CAMERA_IMAGE_WIDTH, CAMERA_IMAGE_HEIGHT)
    }

    private fun copyCameraImage(gameId: String, imageUri: Uri): String? {
        return runCatching {
            val directory = File(filesDir, "camera-images")
            directory.mkdirs()
            val target = File(directory, "${sha1(gameId)}.image")
            contentResolver.openInputStream(imageUri)?.use { input ->
                target.outputStream().use { output ->
                    input.copyTo(output)
                }
            } ?: return@runCatching null
            val bounds = BitmapFactory.Options().apply {
                inJustDecodeBounds = true
            }
            BitmapFactory.decodeFile(target.absolutePath, bounds)
            if (bounds.outWidth <= 0 || bounds.outHeight <= 0) {
                target.delete()
                null
            } else {
                target.absolutePath
            }
        }.getOrNull()
    }

    private fun clearCameraImage() {
        val previousPath = cameraImagePath
        perGameOverrides.clearCameraImagePath(currentGameId)
        cameraImagePath = ""
        controller?.clearCameraImage()
        if (previousPath.isNotBlank()) {
            File(previousPath).delete()
        }
        updateRunButtons()
        Toast.makeText(this, "Camera image cleared", Toast.LENGTH_SHORT).show()
    }

    private fun luxToSolarLevel(lux: Float): Int {
        val normalized = (lux / MAX_SOLAR_LUX).coerceIn(0f, 1f)
        return (normalized * 255f).toInt().coerceIn(0, 255)
    }

    private fun showGamepadSettingsDialog() {
        val sizeLabel = TextView(this).apply {
            setTextColor(getColor(R.color.mgba_text_primary))
        }
        val opacityLabel = TextView(this).apply {
            setTextColor(getColor(R.color.mgba_text_primary))
        }
        val spacingLabel = TextView(this).apply {
            setTextColor(getColor(R.color.mgba_text_primary))
        }
        val sizeSeek = SeekBar(this).apply {
            max = GAMEPAD_SIZE_MAX - GAMEPAD_SIZE_MIN
            progress = virtualGamepadSizePercent - GAMEPAD_SIZE_MIN
        }
        val opacitySeek = SeekBar(this).apply {
            max = GAMEPAD_OPACITY_MAX - GAMEPAD_OPACITY_MIN
            progress = virtualGamepadOpacityPercent - GAMEPAD_OPACITY_MIN
        }
        val spacingSeek = SeekBar(this).apply {
            max = GAMEPAD_SPACING_MAX - GAMEPAD_SPACING_MIN
            progress = virtualGamepadSpacingPercent - GAMEPAD_SPACING_MIN
        }
        val hapticsCheck = CheckBox(this).apply {
            text = "Haptic feedback"
            isChecked = virtualGamepadHapticsEnabled
            setTextColor(getColor(R.color.mgba_text_primary))
            setOnCheckedChangeListener { _, checked ->
                virtualGamepadHapticsEnabled = checked
                applyGamepadStyle()
                saveGamepadStylePreference()
            }
        }
        val leftHandedCheck = CheckBox(this).apply {
            text = "Left-handed layout"
            isChecked = virtualGamepadLeftHanded
            setTextColor(getColor(R.color.mgba_text_primary))
            setOnCheckedChangeListener { _, checked ->
                virtualGamepadLeftHanded = checked
                applyGamepadStyle()
                saveGamepadStylePreference()
            }
        }
        fun updateLabels() {
            sizeLabel.text = "Size: $virtualGamepadSizePercent%"
            opacityLabel.text = "Opacity: $virtualGamepadOpacityPercent%"
            spacingLabel.text = "Spacing: $virtualGamepadSpacingPercent%"
        }
        val listener = object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
                if (seekBar === sizeSeek) {
                    virtualGamepadSizePercent = GAMEPAD_SIZE_MIN + progress
                } else if (seekBar === opacitySeek) {
                    virtualGamepadOpacityPercent = GAMEPAD_OPACITY_MIN + progress
                } else if (seekBar === spacingSeek) {
                    virtualGamepadSpacingPercent = GAMEPAD_SPACING_MIN + progress
                }
                applyGamepadStyle()
                saveGamepadStylePreference()
                updateLabels()
            }

            override fun onStartTrackingTouch(seekBar: SeekBar) = Unit

            override fun onStopTrackingTouch(seekBar: SeekBar) = Unit
        }
        sizeSeek.setOnSeekBarChangeListener(listener)
        opacitySeek.setOnSeekBarChangeListener(listener)
        spacingSeek.setOnSeekBarChangeListener(listener)
        updateLabels()

        val content = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(16), dp(8), dp(16), dp(4))
            addView(sizeLabel)
            addView(sizeSeek)
            addView(opacityLabel)
            addView(opacitySeek)
            addView(spacingLabel)
            addView(spacingSeek)
            addView(hapticsCheck)
            addView(leftHandedCheck)
        }
        AlertDialog.Builder(this)
            .setTitle("Virtual gamepad")
            .setView(content)
            .setNeutralButton("Reset") { _, _ -> resetGamepadStyle() }
            .setPositiveButton("Close", null)
            .show()
    }

    private fun resetGamepadStyle() {
        virtualGamepadSizePercent = DEFAULT_GAMEPAD_SIZE_PERCENT
        virtualGamepadOpacityPercent = DEFAULT_GAMEPAD_OPACITY_PERCENT
        virtualGamepadSpacingPercent = DEFAULT_GAMEPAD_SPACING_PERCENT
        virtualGamepadHapticsEnabled = true
        virtualGamepadLeftHanded = false
        applyGamepadStyle()
        saveGamepadStylePreference()
        Toast.makeText(this, "Virtual gamepad reset", Toast.LENGTH_SHORT).show()
    }

    private fun applyGamepadStyle() {
        gamepadView?.setStyle(
            virtualGamepadSizePercent,
            virtualGamepadOpacityPercent,
            virtualGamepadSpacingPercent,
            virtualGamepadHapticsEnabled,
            virtualGamepadLeftHanded,
        )
    }

    private fun showInputMappingDialog() {
        val profile = inputMappingStore.profile(currentGameId, activeInputDeviceDescriptor)
        val rows = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(8), dp(4), dp(8), dp(4))
        }
        GbaButtons.All.forEach { button ->
            rows.addView(Button(this).apply {
                text = "${button.label}: ${formatKeyCode(profile.keyCodeForMask(button.mask))}"
                setOnClickListener {
                    beginHardwareKeyCapture(button.mask, button.label)
                }
            })
        }
        rows.addView(Button(this).apply {
            text = "Export Profile"
            setOnClickListener {
                inputMappingDialog?.dismiss()
                openInputProfileExportPicker()
            }
        })
        rows.addView(Button(this).apply {
            text = "Import Profile"
            setOnClickListener {
                inputMappingDialog?.dismiss()
                openInputProfileImportPicker()
            }
        })
        rows.addView(Button(this).apply {
            text = "Input Debug"
            setOnClickListener {
                inputMappingDialog?.dismiss()
                showInputDebugDialog()
            }
        })
        val dialog = AlertDialog.Builder(this)
            .setTitle("Hardware keys")
            .setMessage(inputMappingScopeLabel())
            .setView(ScrollView(this).apply { addView(rows) })
            .setNeutralButton("Reset") { _, _ -> resetHardwareKeyMappings() }
            .setNegativeButton("Close", null)
            .show()
        inputMappingDialog = dialog
        dialog.setOnDismissListener {
            if (inputMappingDialog === dialog) {
                inputMappingDialog = null
            }
        }
    }

    private fun beginHardwareKeyCapture(mask: Int, label: String) {
        pendingHardwareMappingMask = mask
        inputMappingDialog?.dismiss()
        val dialog = AlertDialog.Builder(this)
            .setTitle("Map $label")
            .setMessage("Press a hardware key. Press Back to cancel.")
            .setNegativeButton("Cancel") { _, _ -> pendingHardwareMappingMask = 0 }
            .create()
        keyCaptureDialog = dialog
        dialog.setOnKeyListener { _, keyCode, event ->
            if (event.action == KeyEvent.ACTION_DOWN) {
                captureHardwareMappingKey(event)
            }
            true
        }
        dialog.setOnDismissListener {
            if (keyCaptureDialog === dialog) {
                keyCaptureDialog = null
            }
            pendingHardwareMappingMask = 0
        }
        dialog.show()
    }

    private fun captureHardwareMappingKey(event: KeyEvent) {
        val keyCode = event.keyCode
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            cancelHardwareMappingCapture()
            return
        }
        rememberInputDevice(event)
        val mask = pendingHardwareMappingMask
        if (inputMappingStore.setKeyCode(currentGameId, activeInputDeviceDescriptor, mask, keyCode)) {
            clearInput()
            Toast.makeText(
                this,
                "${GbaButtons.labelForMask(mask)} mapped to ${formatKeyCode(keyCode)}",
                Toast.LENGTH_SHORT,
            ).show()
        }
        pendingHardwareMappingMask = 0
        keyCaptureDialog?.dismiss()
        keyCaptureDialog = null
        showInputMappingDialog()
    }

    private fun cancelHardwareMappingCapture() {
        pendingHardwareMappingMask = 0
        keyCaptureDialog?.dismiss()
        keyCaptureDialog = null
        showInputMappingDialog()
    }

    private fun resetHardwareKeyMappings() {
        inputMappingStore.reset(currentGameId, activeInputDeviceDescriptor)
        clearInput()
        Toast.makeText(this, "Hardware keys reset", Toast.LENGTH_SHORT).show()
    }

    private fun openInputProfileExportPicker() {
        val intent = Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "application/json"
            putExtra(Intent.EXTRA_TITLE, "mgba-input-profile.json")
            addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
        }
        startActivityForResult(intent, REQUEST_EXPORT_INPUT_PROFILE)
    }

    private fun openInputProfileImportPicker() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "application/json"
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        startActivityForResult(intent, REQUEST_IMPORT_INPUT_PROFILE)
    }

    private fun exportInputProfile(uri: Uri) {
        val json = inputMappingStore.exportProfileJson(
            currentGameId,
            activeInputDeviceDescriptor,
            activeInputDeviceName,
        )
        val ok = runCatching {
            contentResolver.openOutputStream(uri)?.bufferedWriter()?.use { writer ->
                writer.write(json)
            } != null
        }.getOrDefault(false)
        Toast.makeText(this, if (ok) "Input profile exported" else "Input profile export failed", Toast.LENGTH_SHORT).show()
    }

    private fun importInputProfile(uri: Uri) {
        val ok = runCatching {
            val json = contentResolver.openInputStream(uri)?.bufferedReader()?.use { it.readText() } ?: return@runCatching false
            inputMappingStore.importProfileJson(currentGameId, activeInputDeviceDescriptor, json)
        }.getOrDefault(false)
        clearInput()
        Toast.makeText(this, if (ok) "Input profile imported" else "Input profile import failed", Toast.LENGTH_SHORT).show()
    }

    private fun showInputDebugDialog() {
        val content = TextView(this).apply {
            setPadding(dp(16), dp(12), dp(16), dp(12))
            setTextColor(getColor(R.color.mgba_text_primary))
            text = inputDebugSummary()
        }
        AlertDialog.Builder(this)
            .setTitle("Input debug")
            .setView(ScrollView(this).apply { addView(content) })
            .setPositiveButton("Close", null)
            .show()
    }

    private fun inputDebugSummary(): String {
        return String.format(
            Locale.US,
            "Device\nName: %s\nDescriptor: %s\n\nLast key\nAction: %s\nCode: %s\n\nLast axes\nSource: 0x%08X\nX: %.3f\nY: %.3f\nHat X: %.3f\nHat Y: %.3f\nLeft trigger: %.3f\nRight trigger: %.3f\nMapped keys: %s\nDeadzone: %d%%",
            lastInputDeviceName ?: "(none)",
            lastInputDeviceDescriptor ?: "(none)",
            lastInputKeyAction,
            formatKeyCodeWithNumber(lastInputKeyCode),
            lastMotionSource,
            lastAxisX,
            lastAxisY,
            lastHatX,
            lastHatY,
            lastLeftTrigger,
            lastRightTrigger,
            formatGbaMask(lastMotionKeys),
            deadzonePercent,
        )
    }

    private fun rememberLastKeyEvent(event: KeyEvent) {
        lastInputKeyCode = event.keyCode
        lastInputKeyAction = when (event.action) {
            KeyEvent.ACTION_DOWN -> "Down"
            KeyEvent.ACTION_UP -> "Up"
            else -> event.action.toString()
        }
        rememberLastInputDevice(event.device?.descriptor, event.device?.name)
    }

    private fun rememberLastMotionEvent(event: MotionEvent, keys: Int) {
        rememberLastInputDevice(event.device?.descriptor, event.device?.name)
        lastMotionSource = event.source
        lastMotionKeys = keys
        lastAxisX = event.getAxisValue(MotionEvent.AXIS_X)
        lastAxisY = event.getAxisValue(MotionEvent.AXIS_Y)
        lastHatX = event.getAxisValue(MotionEvent.AXIS_HAT_X)
        lastHatY = event.getAxisValue(MotionEvent.AXIS_HAT_Y)
        lastLeftTrigger = max(
            event.getAxisValue(MotionEvent.AXIS_LTRIGGER),
            event.getAxisValue(MotionEvent.AXIS_BRAKE),
        )
        lastRightTrigger = max(
            event.getAxisValue(MotionEvent.AXIS_RTRIGGER),
            event.getAxisValue(MotionEvent.AXIS_GAS),
        )
    }

    private fun rememberLastInputDevice(descriptor: String?, name: String?) {
        lastInputDeviceDescriptor = descriptor?.takeIf { it.isNotBlank() } ?: lastInputDeviceDescriptor
        lastInputDeviceName = name?.takeIf { it.isNotBlank() } ?: lastInputDeviceName
    }

    private fun rememberInputDevice(event: KeyEvent) {
        val descriptor = event.deviceDescriptor() ?: return
        activeInputDeviceDescriptor = descriptor
        activeInputDeviceName = event.device?.name
    }

    private fun rememberInputDevice(event: MotionEvent) {
        val descriptor = event.deviceDescriptor() ?: return
        activeInputDeviceDescriptor = descriptor
        activeInputDeviceName = event.device?.name
    }

    private fun KeyEvent.deviceDescriptor(): String? {
        return device?.descriptor?.takeIf { it.isNotBlank() }
    }

    private fun MotionEvent.deviceDescriptor(): String? {
        return device?.descriptor?.takeIf { it.isNotBlank() }
    }

    private fun isGamepadMotionSource(source: Int): Boolean {
        return (source and android.view.InputDevice.SOURCE_JOYSTICK) == android.view.InputDevice.SOURCE_JOYSTICK ||
            (source and android.view.InputDevice.SOURCE_GAMEPAD) == android.view.InputDevice.SOURCE_GAMEPAD
    }

    private fun inputMappingScopeLabel(): String {
        activeInputDeviceName?.let { return "Bindings apply to $it." }
        return if (currentGameId == null) "Bindings apply globally." else "Bindings apply to this game."
    }

    private fun formatKeyCode(keyCode: Int?): String {
        return keyCode?.let {
            KeyEvent.keyCodeToString(it).removePrefix("KEYCODE_")
        }.orEmpty()
    }

    private fun formatKeyCodeWithNumber(keyCode: Int?): String {
        return keyCode?.let { "${formatKeyCode(it)} ($it)" } ?: "(none)"
    }

    private fun formatGbaMask(mask: Int): String {
        if (mask == 0) {
            return "(none)"
        }
        return GbaButtons.All
            .filter { (mask and it.mask) != 0 }
            .joinToString("+") { it.label }
    }

    private fun startStatsOverlay() {
        statsOverlay?.visibility = View.VISIBLE
        lastStatsFrames = 0L
        lastStatsAtMs = 0L
        updateStatsOverlay()
        statsHandler.removeCallbacks(statsRunnable)
        statsHandler.postDelayed(statsRunnable, 1000L)
    }

    private fun stopStatsOverlay() {
        showStats = false
        statsHandler.removeCallbacks(statsRunnable)
        statsOverlay?.visibility = View.GONE
        updateRunButtons()
    }

    private fun updateStatsOverlay() {
        val stats = controller?.stats() ?: return
        val now = SystemClock.elapsedRealtime()
        val frameDelta = (stats.frames - lastStatsFrames).coerceAtLeast(0L)
        val elapsedMs = now - lastStatsAtMs
        val fps = if (lastStatsAtMs > 0L && elapsedMs > 0L) {
            frameDelta * 1000.0 / elapsedMs
        } else {
            0.0
        }
        val frameTimeMs = if (lastStatsAtMs > 0L && frameDelta > 0L) elapsedMs.toDouble() / frameDelta else 0.0
        lastStatsFrames = stats.frames
        lastStatsAtMs = now
        statsOverlay?.text = String.format(
            Locale.US,
            "FPS %.1f  Frame %.2fms\nFrames %d\nROM %s  %s\nVideo %dx%d\nRun %s  Fast %s  Rewind %s/%d/%d  Skip %d\nAudio %s  Vol %d%%  Buf %d  LPF %d  Und %d\nScale %s  Filter %s  BIOS %s",
            fps,
            frameTimeMs,
            stats.frames,
            stats.romPlatform.ifBlank { "unknown" },
            stats.gameTitle.ifBlank { "untitled" },
            stats.videoWidth,
            stats.videoHeight,
            if (stats.running && !stats.paused) "on" else "off",
            if (stats.fastForward) FastForwardModes.labelForMultiplier(stats.fastForwardMultiplier) else "off",
            if (stats.rewinding) "on" else if (stats.rewindEnabled) "ready" else "off",
            stats.rewindBufferCapacity,
            stats.rewindBufferInterval,
            frameSkip,
            if (muted) "muted" else "on",
            stats.volumePercent,
            stats.audioBufferSamples,
            stats.audioLowPassRange,
            stats.audioUnderruns,
            SCALE_LABELS.getOrElse(stats.scaleMode) { SCALE_LABELS[0] },
            FILTER_LABELS.getOrElse(stats.filterMode) { FILTER_LABELS[0] },
            if (stats.skipBios) "skip" else "boot",
        )
    }

    private fun startRumblePolling() {
        rumbleHandler.removeCallbacks(rumbleRunnable)
        rumbleHandler.post(rumbleRunnable)
    }

    private fun stopRumblePolling() {
        rumbleHandler.removeCallbacks(rumbleRunnable)
    }

    private fun pollRumble() {
        if (!rumbleEnabled) {
            return
        }
        if (controller?.pollRumble() == true) {
            triggerRumblePulse()
        }
    }

    private fun triggerRumblePulse() {
        val vibrator = vibrator ?: return
        if (!vibrator.hasVibrator()) {
            return
        }
        val now = SystemClock.elapsedRealtime()
        if (now - lastRumbleAtMs < RUMBLE_INTERVAL_MS) {
            return
        }
        lastRumbleAtMs = now
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            vibrator.vibrate(VibrationEffect.createOneShot(RUMBLE_PULSE_MS, VibrationEffect.DEFAULT_AMPLITUDE))
        } else {
            @Suppress("DEPRECATION")
            vibrator.vibrate(RUMBLE_PULSE_MS)
        }
    }

    private fun saveStateWithConfirmation() {
        if (controller?.hasStateSlot(stateSlot) != true) {
            saveStateNow()
            return
        }
        AlertDialog.Builder(this)
            .setTitle("Overwrite state?")
            .setMessage("Slot $stateSlot already has a save state.")
            .setPositiveButton("Overwrite") { _, _ -> saveStateNow() }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun deleteStateWithConfirmation() {
        if (controller?.hasStateSlot(stateSlot) != true) {
            Toast.makeText(this, "No state in slot", Toast.LENGTH_SHORT).show()
            return
        }
        AlertDialog.Builder(this)
            .setTitle("Delete state?")
            .setMessage("Slot $stateSlot will be removed.")
            .setPositiveButton("Delete") { _, _ ->
                val ok = controller?.deleteStateSlot(stateSlot) == true
                if (ok) {
                    deleteStateThumbnail(stateSlot)
                    updateStateThumbnail()
                }
                Toast.makeText(this, if (ok) "State deleted" else "Delete failed", Toast.LENGTH_SHORT).show()
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun openStateExportPicker() {
        if (controller?.hasStateSlot(stateSlot) != true) {
            Toast.makeText(this, "No state in slot", Toast.LENGTH_SHORT).show()
            return
        }
        pendingExportStateSlot = stateSlot
        val intent = Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "application/octet-stream"
            putExtra(Intent.EXTRA_TITLE, "mgba-slot-$stateSlot.ss")
            addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
        }
        startActivityForResult(intent, REQUEST_EXPORT_STATE)
    }

    private fun exportStateSlot(uri: Uri, slot: Int) {
        val ok = runCatching {
            contentResolver.openFileDescriptor(uri, "w")?.use { descriptor ->
                controller?.exportStateSlotFd(slot, descriptor.fd) == true
            } == true
        }.getOrDefault(false)
        Toast.makeText(this, if (ok) "State exported" else "State export failed", Toast.LENGTH_SHORT).show()
    }

    private fun importStateWithConfirmation() {
        if (controller?.hasStateSlot(stateSlot) != true) {
            openStateImportPicker()
            return
        }
        AlertDialog.Builder(this)
            .setTitle("Overwrite state?")
            .setMessage("Slot $stateSlot already has a save state.")
            .setPositiveButton("Overwrite") { _, _ -> openStateImportPicker() }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun openStateImportPicker() {
        pendingImportStateSlot = stateSlot
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        startActivityForResult(intent, REQUEST_IMPORT_STATE)
    }

    private fun importStateSlot(uri: Uri, slot: Int) {
        val ok = runCatching {
            contentResolver.openFileDescriptor(uri, "r")?.use { descriptor ->
                controller?.importStateSlotFd(slot, descriptor.fd) == true
            } == true
        }.getOrDefault(false)
        if (ok) {
            deleteStateThumbnail(slot)
            updateStateThumbnail()
        }
        Toast.makeText(this, if (ok) "State imported" else "State import failed", Toast.LENGTH_SHORT).show()
    }

    private fun saveStateNow() {
        val ok = controller?.saveStateSlot(stateSlot) == true
        if (ok) {
            val thumbnailUpdated = controller?.takeScreenshot()?.let { recordStateThumbnail(it, stateSlot) } == true
            if (thumbnailUpdated) {
                updateStateThumbnail()
            }
        }
        Toast.makeText(this, if (ok) "State saved" else "Save failed", Toast.LENGTH_SHORT).show()
    }

    private fun recordStateThumbnail(sourcePath: String, slot: Int): Boolean {
        val target = stateThumbnailFile(slot, forWrite = true) ?: return false
        return runCatching {
            target.parentFile?.mkdirs()
            File(sourcePath).copyTo(target, overwrite = true)
            File(sourcePath).delete()
            true
        }.getOrDefault(false)
    }

    private fun deleteStateThumbnail(slot: Int) {
        artifactGameIds().forEach { gameId ->
            stateThumbnailFileForGame(gameId, slot).delete()
        }
    }

    private fun stateThumbnailFile(slot: Int, forWrite: Boolean = false): File? {
        if (!forWrite) {
            artifactGameIds()
                .asSequence()
                .map { stateThumbnailFileForGame(it, slot) }
                .firstOrNull { it.isFile }
                ?.let { return it }
        }
        val gameId = artifactGameId() ?: return null
        return stateThumbnailFileForGame(gameId, slot)
    }

    private fun stateThumbnailFileForGame(gameId: String, slot: Int): File {
        return File(File(filesDir, "state-thumbnails"), "${sha1(gameId)}-slot-$slot.png")
    }

    private fun artifactGameId(): String? {
        return currentStableGameId?.takeIf { it.isNotBlank() }
            ?: currentGameId?.takeIf { it.isNotBlank() }
    }

    private fun artifactGameIds(): List<String> {
        return listOfNotNull(
            currentStableGameId?.takeIf { it.isNotBlank() },
            currentGameId?.takeIf { it.isNotBlank() },
        ).distinct()
    }

    private fun patchGameId(): String? {
        return artifactGameIds().firstOrNull { patchStore.fileForGame(it) != null } ?: artifactGameId()
    }

    private fun cheatGameId(): String? {
        return artifactGameIds().firstOrNull { cheatStore.fileForGame(it) != null } ?: artifactGameId()
    }

    private fun clearPatchArtifacts(): Boolean {
        val ids = artifactGameIds()
        return ids.isNotEmpty() && ids.map { patchStore.clearForGame(it) }.all { it }
    }

    private fun clearCheatArtifacts(): Boolean {
        val ids = artifactGameIds()
        return ids.isNotEmpty() && ids.map { cheatStore.clearForGame(it) }.all { it }
    }

    private fun sha1(value: String): String {
        val bytes = MessageDigest.getInstance("SHA-1").digest(value.toByteArray(Charsets.UTF_8))
        return bytes.joinToString("") { "%02x".format(it.toInt() and 0xff) }
    }

    private fun shareScreenshot(path: String) {
        val uri = ScreenshotShareProvider.uriFor(this, path)
        val intent = Intent(Intent.ACTION_SEND).apply {
            type = "image/png"
            putExtra(Intent.EXTRA_STREAM, uri)
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        startActivity(Intent.createChooser(intent, "Share screenshot"))
    }

    private fun recordScreenshotCover(path: String) {
        val gameId = currentGameId ?: return
        runCatching {
            RomLibraryStore(this).setCoverPath(Uri.parse(gameId), path)
        }
    }

    private fun screenshotExportName(): String {
        val base = EmulatorSession.currentGame()?.displayName
            ?.substringBeforeLast('.')
            ?.replace(Regex("[^A-Za-z0-9._-]+"), "-")
            ?.trim('-')
            ?.ifBlank { null }
            ?: "mgba-screenshot"
        return "$base-${System.currentTimeMillis()}.png"
    }

    private fun openSaveImportPicker() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        startActivityForResult(intent, REQUEST_IMPORT_SAVE)
    }

    private fun openCheatImportPicker() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        startActivityForResult(intent, REQUEST_IMPORT_CHEATS)
    }

    private fun showCheatActionsDialog() {
        val gameId = cheatGameId()
        val entries = cheatStore.entriesForGame(gameId)
        val labels = buildList {
            entries.forEach { entry ->
                val state = if (entry.enabled) "[x]" else "[ ]"
                val preview = entry.lines.firstOrNull()?.let { "  $it" }.orEmpty()
                add("$state ${entry.name}$preview")
            }
            add("Add code")
            add("Import file")
        }.toTypedArray()
        AlertDialog.Builder(this)
            .setTitle("Cheats")
            .setItems(labels) { _, which ->
                when {
                    which < entries.size -> toggleCheat(which, !entries[which].enabled)
                    which == entries.size -> showAddCheatDialog()
                    else -> openCheatImportPicker()
                }
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun showAddCheatDialog() {
        val nameInput = EditText(this).apply {
            hint = "Name"
            setSingleLine(true)
            inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
        }
        val codeInput = EditText(this).apply {
            hint = "Code lines"
            minLines = 4
            inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_MULTI_LINE or InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
        }
        val body = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(16), 0, dp(16), 0)
            addView(nameInput)
            addView(codeInput)
        }
        AlertDialog.Builder(this)
            .setTitle("Add cheat")
            .setView(body)
            .setPositiveButton("Save") { _, _ ->
                val stored = cheatStore.addManual(cheatGameId(), nameInput.text.toString(), codeInput.text.toString())
                val applied = stored && applyStoredCheats()
                val message = when {
                    applied -> "Cheat saved"
                    stored -> "Cheat saved; apply failed"
                    else -> "Cheat save failed"
                }
                Toast.makeText(this, message, Toast.LENGTH_SHORT).show()
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun toggleCheat(index: Int, enabled: Boolean) {
        val stored = cheatStore.setEnabled(cheatGameId(), index, enabled)
        val applied = stored && applyStoredCheats()
        val message = when {
            applied -> if (enabled) "Cheat enabled" else "Cheat disabled"
            stored -> "Cheat saved; apply failed"
            else -> "Cheat update failed"
        }
        Toast.makeText(this, message, Toast.LENGTH_SHORT).show()
    }

    private fun applyStoredCheats(): Boolean {
        val file = artifactGameIds()
            .asSequence()
            .mapNotNull { cheatStore.fileForGame(it) }
            .firstOrNull()
            ?: return false
        return runCatching {
            ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY).use { descriptor ->
                controller?.importCheatsFd(descriptor.fd) == true
            }
        }.getOrDefault(false)
    }

    private fun openPatchImportPicker() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        startActivityForResult(intent, REQUEST_IMPORT_PATCH)
    }

    private fun importBatterySave(uri: Uri) {
        val ok = runCatching {
            contentResolver.openFileDescriptor(uri, "r")?.use { descriptor ->
                controller?.importBatterySaveFd(descriptor.fd) == true
            } == true
        }.getOrDefault(false)
        Toast.makeText(this, if (ok) "Save imported" else "Import failed", Toast.LENGTH_SHORT).show()
    }

    private fun importPatch(uri: Uri) {
        val name = displayName(uri, "patch")
        val gameId = patchGameId()
        val stored = patchStore.importForGame(gameId, uri, name)
        val applied = if (stored) {
            patchStore.fileForGame(gameId)?.let { file ->
                runCatching {
                    ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY).use { descriptor ->
                        controller?.importPatchFd(descriptor.fd) == true
                    }
                }.getOrDefault(false)
            } ?: false
        } else {
            false
        }
        val message = when {
            applied -> "Patch imported"
            stored -> "Patch saved; apply failed"
            else -> "Patch import failed"
        }
        Toast.makeText(this, message, Toast.LENGTH_SHORT).show()
    }

    private fun clearPatchWithConfirmation() {
        AlertDialog.Builder(this)
            .setTitle("Clear patch?")
            .setMessage("Remove the saved patch for this game. Active patch changes may stay until reset or next launch.")
            .setPositiveButton("Clear") { _, _ ->
                val ok = clearPatchArtifacts()
                Toast.makeText(this, if (ok) "Patch cleared" else "Clear failed", Toast.LENGTH_SHORT).show()
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun exitWithConfirmation() {
        AlertDialog.Builder(this)
            .setTitle("Exit game?")
            .setMessage("Save data will be flushed before the emulator closes.")
            .setPositiveButton("Exit") { _, _ ->
                finish()
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun importCheats(uri: Uri) {
        val name = displayName(uri, "cheats")
        val stored = cheatStore.importForGame(cheatGameId(), uri, name)
        val applied = stored && applyStoredCheats()
        val message = when {
            applied -> "Cheats imported"
            stored -> "Cheats saved; apply failed"
            else -> "Cheat import failed"
        }
        Toast.makeText(this, message, Toast.LENGTH_SHORT).show()
    }

    private fun clearCheatsWithConfirmation() {
        AlertDialog.Builder(this)
            .setTitle("Clear cheats?")
            .setMessage("Remove saved cheats for this game. Active cheats may stay until reset or next launch.")
            .setPositiveButton("Clear") { _, _ ->
                val ok = clearCheatArtifacts()
                Toast.makeText(this, if (ok) "Cheats cleared" else "Clear failed", Toast.LENGTH_SHORT).show()
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun displayName(uri: Uri, fallback: String): String {
        contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { cursor ->
            if (cursor.moveToFirst()) {
                return cursor.getString(0) ?: uri.lastPathSegment ?: fallback
            }
        }
        return uri.lastPathSegment ?: fallback
    }

    private fun dp(value: Int): Int {
        return (value * resources.displayMetrics.density).toInt()
    }

    private fun applyOrientationMode() {
        requestedOrientation = when (orientationMode) {
            1 -> ActivityInfo.SCREEN_ORIENTATION_USER_LANDSCAPE
            2 -> ActivityInfo.SCREEN_ORIENTATION_USER_PORTRAIT
            else -> ActivityInfo.SCREEN_ORIENTATION_USER
        }
    }

    private fun enterImmersiveMode() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            window.insetsController?.let { controller ->
                controller.hide(WindowInsets.Type.statusBars() or WindowInsets.Type.navigationBars())
                controller.systemBarsBehavior = WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            }
        } else {
            @Suppress("DEPRECATION")
            window.decorView.systemUiVisibility = (
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                    or View.SYSTEM_UI_FLAG_FULLSCREEN
                    or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                )
        }
    }

    private data class CameraImagePixels(
        val pixels: IntArray,
        val width: Int,
        val height: Int,
    )

    companion object {
        private const val REQUEST_IMPORT_SAVE = 2001
        private const val REQUEST_IMPORT_CHEATS = 2002
        private const val REQUEST_EXPORT_STATE = 2003
        private const val REQUEST_IMPORT_STATE = 2004
        private const val REQUEST_EXPORT_INPUT_PROFILE = 2005
        private const val REQUEST_IMPORT_INPUT_PROFILE = 2006
        private const val REQUEST_IMPORT_PATCH = 2007
        private const val REQUEST_IMPORT_CAMERA_IMAGE = 2008
        private const val RUMBLE_POLL_MS = 50L
        private const val RUMBLE_INTERVAL_MS = 90L
        private const val RUMBLE_PULSE_MS = 45L
        private const val CAMERA_IMAGE_WIDTH = 128
        private const val CAMERA_IMAGE_HEIGHT = 112
        private const val MAX_GYRO_RADIANS = 8f
        private const val MAX_SOLAR_LUX = 10000f
        private const val DEFAULT_GAMEPAD_SIZE_PERCENT = 100
        private const val DEFAULT_GAMEPAD_OPACITY_PERCENT = 100
        private const val DEFAULT_GAMEPAD_SPACING_PERCENT = 100
        private const val GAMEPAD_SIZE_MIN = 60
        private const val GAMEPAD_SIZE_MAX = 140
        private const val GAMEPAD_OPACITY_MIN = 35
        private const val GAMEPAD_OPACITY_MAX = 100
        private const val GAMEPAD_SPACING_MIN = 70
        private const val GAMEPAD_SPACING_MAX = 140
        private val DEADZONE_LEVELS = arrayOf(25, 35, 45, 55, 65)
        private val VOLUME_LEVELS = arrayOf(100, 75, 50, 25)
        private val FRAME_SKIP_LABELS = arrayOf("Skip0", "Skip1", "Skip2", "Skip3")
        private val SCALE_LABELS = arrayOf("Fit", "Fill", "Int", "Orig", "Str")
        private val FILTER_LABELS = arrayOf("Pix", "Smooth")
        private val ORIENTATION_LABELS = arrayOf("Rot", "Land", "Port")
    }
}
