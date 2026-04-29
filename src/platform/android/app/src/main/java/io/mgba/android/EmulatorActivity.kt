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
import android.media.AudioDeviceCallback
import android.media.AudioDeviceInfo
import android.media.AudioManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.ParcelFileDescriptor
import android.os.SystemClock
import android.os.VibrationEffect
import android.os.Vibrator
import android.provider.MediaStore
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
import android.widget.HorizontalScrollView
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
import io.mgba.android.input.VirtualGamepadLayoutOffsets
import io.mgba.android.input.VirtualGamepadView
import io.mgba.android.library.RomLibraryStore
import io.mgba.android.settings.AudioBufferModes
import io.mgba.android.settings.AudioLowPassModes
import io.mgba.android.settings.EmulatorPreferences
import io.mgba.android.settings.FastForwardModes
import io.mgba.android.settings.InputMappingStore
import io.mgba.android.settings.PerGameOverrideStore
import io.mgba.android.settings.RewindSettings
import io.mgba.android.storage.AppLogStore
import io.mgba.android.storage.CheatEntry
import io.mgba.android.storage.CheatStore
import io.mgba.android.storage.BiosStore
import io.mgba.android.storage.BiosSlot
import io.mgba.android.storage.LogExporter
import io.mgba.android.storage.PatchStore
import io.mgba.android.storage.ScreenshotExporter
import io.mgba.android.storage.ScreenshotShareProvider
import io.mgba.android.storage.SaveExporter
import java.io.File
import java.security.MessageDigest
import java.util.Locale
import java.util.zip.ZipEntry
import java.util.zip.ZipInputStream
import java.util.zip.ZipOutputStream
import kotlin.math.max
import kotlin.math.min
import org.json.JSONObject

class EmulatorActivity : Activity(), SurfaceHolder.Callback, SensorEventListener {
    private var controller: EmulatorController? = null
    private var videoSurface: AspectRatioSurfaceView? = null
    private var gamepadView: VirtualGamepadView? = null
    private lateinit var preferences: EmulatorPreferences
    private lateinit var perGameOverrides: PerGameOverrideStore
    private lateinit var inputMappingStore: InputMappingStore
    private lateinit var cheatStore: CheatStore
    private lateinit var biosStore: BiosStore
    private lateinit var patchStore: PatchStore
    private var currentGameId: String? = null
    private var currentStableGameId: String? = null
    private var currentOverrideGameId: String? = null
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
    private var lastInputSyncSource = "None"
    private var lastInputSyncEventAgeUs = 0L
    private var lastInputSyncDurationUs = 0L
    private var maxInputSyncDurationUs = 0L
    private var inputSyncSamples = 0L
    private var slowInputSyncSamples = 0L
    private var lastInputSyncKeys = 0
    private var lastInputSyncAtMs = 0L
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
    private var virtualGamepadLayoutOffsets = VirtualGamepadLayoutOffsets()
    private var gamepadLayoutEditing = false
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
    private var firstFrameLogged = false
    private var lastStatsFrames = 0L
    private var lastStatsAtMs = 0L
    private var pendingExportStateSlot = 1
    private var pendingImportStateSlot = 1
    private var pendingGameBiosSlot = BiosSlot.Default
    private var pendingHardwareMappingMask = 0
    private var pendingExportSavePath = ""
    private var playAccountingStartedAtMs = 0L
    private var scaleMode = 0
    private var videoAspectWidth = 0
    private var videoAspectHeight = 0
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
    private val audioManager: AudioManager? by lazy { getSystemService(AudioManager::class.java) }
    private val accelerometer: Sensor? by lazy { sensorManager?.getDefaultSensor(Sensor.TYPE_ACCELEROMETER) }
    private val gyroscope: Sensor? by lazy { sensorManager?.getDefaultSensor(Sensor.TYPE_GYROSCOPE) }
    private val lightSensor: Sensor? by lazy { sensorManager?.getDefaultSensor(Sensor.TYPE_LIGHT) }
    private var audioRouteCallbackRegistered = false
    private var lastAudioRouteSignature: String? = null
    private var lastRumbleAtMs = 0L
    private val audioRouteRestartRunnable = Runnable {
        restartAudioAfterRouteChange()
    }
    private val firstFrameRunnable = object : Runnable {
        override fun run() {
            pollFirstFrameTiming()
        }
    }
    private val audioRouteCallback = object : AudioDeviceCallback() {
        override fun onAudioDevicesAdded(addedDevices: Array<out AudioDeviceInfo>) {
            handleAudioRouteChange("added", addedDevices)
        }

        override fun onAudioDevicesRemoved(removedDevices: Array<out AudioDeviceInfo>) {
            handleAudioRouteChange("removed", removedDevices)
        }
    }
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
        biosStore = BiosStore(this)
        patchStore = PatchStore(this)
        val currentGame = EmulatorSession.currentGame()
        currentGameId = currentGame?.uri
        currentStableGameId = currentGame?.stableId?.takeIf { it.isNotBlank() }
        currentOverrideGameId = currentStableGameId ?: currentGameId
        perGameOverrides.migrateGameId(currentOverrideGameId, currentGameId)
        inputMappingStore.migrateGameId(currentOverrideGameId, currentGameId)
        loadPerGameOverridesFromStore()
        autoStateOnExit = preferences.autoStateOnExit
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        applyOrientationMode()
        controller = EmulatorSession.current()
        if (controller == null) {
            finish()
            return
        }
        applyPerGameOverridesToRuntime()

        val root = FrameLayout(this).apply {
            setBackgroundColor(getColor(R.color.mgba_background))
        }

        val surface = AspectRatioSurfaceView(this).apply {
            holder.addCallback(this@EmulatorActivity)
        }
        videoSurface = surface
        root.addView(
            surface,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT,
                android.view.Gravity.CENTER,
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
            setLayoutOffsets(virtualGamepadLayoutOffsets)
            setOnKeysChangedListener { keys, eventTimeMs ->
                virtualKeys = keys
                syncKeys(eventTimeMs, "Virtual")
            }
            setOnLayoutOffsetsChangedListener { offsets ->
                virtualGamepadLayoutOffsets = offsets
                saveGamepadLayoutPreference()
            }
            setOnSizePercentChangedListener { sizePercent ->
                virtualGamepadSizePercent = sizePercent
                saveGamepadStylePreference()
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
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.WRAP_CONTENT,
                android.view.Gravity.TOP,
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
        enterImmersiveMode()
        startFirstFrameTiming()
    }

    override fun onResume() {
        super.onResume()
        enterImmersiveMode()
        registerAudioRouteCallback()
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
        if (requestCode == REQUEST_CAPTURE_CAMERA_IMAGE) {
            importCapturedCameraImage(data)
            return
        }
        val uri = data?.data ?: return
        when (requestCode) {
            REQUEST_IMPORT_SAVE -> importBatterySave(uri)
            REQUEST_IMPORT_CHEATS -> importCheats(uri)
            REQUEST_EXPORT_STATE -> exportStateSlot(uri, pendingExportStateSlot)
            REQUEST_EXPORT_GAME_DATA -> exportGameDataPackage(uri)
            REQUEST_IMPORT_GAME_DATA -> importGameDataPackage(uri)
            REQUEST_IMPORT_STATE -> importStateSlot(uri, pendingImportStateSlot)
            REQUEST_EXPORT_INPUT_PROFILE -> exportInputProfile(uri)
            REQUEST_IMPORT_INPUT_PROFILE -> importInputProfile(uri)
            REQUEST_IMPORT_PATCH -> importPatch(uri)
            REQUEST_IMPORT_CAMERA_IMAGE -> importCameraImage(uri)
            REQUEST_IMPORT_GAME_BIOS -> importGameBios(uri)
            REQUEST_EXPORT_DIAGNOSTICS -> exportDiagnosticsToUri(uri)
            REQUEST_EXPORT_SAVE -> exportBatterySaveToUri(uri)
            REQUEST_EXPORT_SCREENSHOT -> exportScreenshotToUri(uri)
        }
    }

    override fun onPause() {
        clearInput()
        recordPlayTime()
        unregisterAudioRouteCallback()
        stopRumblePolling()
        unregisterSensors()
        stopStatsOverlay()
        controller?.pause()
        super.onPause()
    }

    override fun onDestroy() {
        clearInput()
        recordPlayTime()
        statsHandler.removeCallbacks(firstFrameRunnable)
        statsHandler.removeCallbacks(audioRouteRestartRunnable)
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

    private fun startFirstFrameTiming() {
        val launchStartedAtMs = EmulatorSession.currentGame()?.launchStartedAtMs ?: 0L
        if (launchStartedAtMs <= 0L) {
            return
        }
        firstFrameLogged = false
        statsHandler.removeCallbacks(firstFrameRunnable)
        statsHandler.post(firstFrameRunnable)
    }

    private fun pollFirstFrameTiming() {
        if (firstFrameLogged) {
            return
        }
        val game = EmulatorSession.currentGame() ?: return
        val launchStartedAtMs = game.launchStartedAtMs
        if (launchStartedAtMs <= 0L) {
            return
        }
        val nowMs = SystemClock.elapsedRealtime()
        val stats = controller?.stats()
        if (stats != null && stats.frames > 0L && stats.videoWidth > 0 && stats.videoHeight > 0) {
            firstFrameLogged = true
            updateVideoAspectRatio(stats.videoWidth, stats.videoHeight)
            val loadToFirstFrameMs = nowMs - launchStartedAtMs
            val loadedToFirstFrameMs = if (game.loadedAtMs > 0L) nowMs - game.loadedAtMs else -1L
            AppLogStore.append(
                this,
                "First frame ${game.displayName}: loadToFirstFrameMs=$loadToFirstFrameMs, loadedToFirstFrameMs=$loadedToFirstFrameMs, nativeFrames=${stats.frames}, video=${stats.videoWidth}x${stats.videoHeight}",
            )
            return
        }
        if (nowMs - launchStartedAtMs < FIRST_FRAME_TIMEOUT_MS) {
            statsHandler.postDelayed(firstFrameRunnable, FIRST_FRAME_POLL_MS)
        } else {
            firstFrameLogged = true
            AppLogStore.append(this, "First frame timeout ${game.displayName}: waitedMs=${nowMs - launchStartedAtMs}")
        }
    }

    private fun registerAudioRouteCallback() {
        if (audioRouteCallbackRegistered) {
            return
        }
        if (audioManager == null) {
            return
        }
        runCatching {
            lastAudioRouteSignature = currentAudioRouteSignature()
            audioManager?.registerAudioDeviceCallback(audioRouteCallback, statsHandler)
            audioRouteCallbackRegistered = true
        }.onFailure { error ->
            AppLogStore.append(this, "Audio route callback registration failed: ${error.javaClass.simpleName}")
        }
    }

    private fun unregisterAudioRouteCallback() {
        if (!audioRouteCallbackRegistered) {
            return
        }
        statsHandler.removeCallbacks(audioRouteRestartRunnable)
        runCatching {
            audioManager?.unregisterAudioDeviceCallback(audioRouteCallback)
        }.onFailure { error ->
            AppLogStore.append(this, "Audio route callback unregistration failed: ${error.javaClass.simpleName}")
        }
        audioRouteCallbackRegistered = false
        lastAudioRouteSignature = null
    }

    private fun handleAudioRouteChange(action: String, devices: Array<out AudioDeviceInfo>) {
        val outputs = devices.filter { it.isSink }
        if (outputs.isEmpty()) {
            return
        }
        val currentSignature = currentAudioRouteSignature()
        val previousSignature = lastAudioRouteSignature
        lastAudioRouteSignature = currentSignature
        if (previousSignature != null && currentSignature == previousSignature) {
            AppLogStore.append(this, "Audio route observed: ${outputs.joinToString { audioDeviceLabel(it) }}")
            return
        }
        AppLogStore.append(this, "Audio route $action: ${outputs.joinToString { audioDeviceLabel(it) }}")
        statsHandler.removeCallbacks(audioRouteRestartRunnable)
        if (!muted) {
            statsHandler.postDelayed(audioRouteRestartRunnable, AUDIO_ROUTE_RESTART_DELAY_MS)
        }
    }

    private fun restartAudioAfterRouteChange() {
        if (muted) {
            return
        }
        controller?.restartAudioOutput()
        controller?.setAudioEnabled(true)
        controller?.setVolumePercent(volumePercent)
        controller?.setAudioBufferSamples(AudioBufferModes.samplesFor(audioBufferMode))
        controller?.setLowPassRangePercent(AudioLowPassModes.rangeFor(audioLowPassMode))
        AppLogStore.append(this, "Audio output restarted after route change")
    }

    private fun audioDeviceLabel(device: AudioDeviceInfo): String {
        val type = when (device.type) {
            AudioDeviceInfo.TYPE_BUILTIN_EARPIECE -> "earpiece"
            AudioDeviceInfo.TYPE_BUILTIN_SPEAKER -> "speaker"
            AudioDeviceInfo.TYPE_WIRED_HEADPHONES -> "wired-headphones"
            AudioDeviceInfo.TYPE_WIRED_HEADSET -> "wired-headset"
            AudioDeviceInfo.TYPE_BLUETOOTH_A2DP -> "bluetooth-a2dp"
            AudioDeviceInfo.TYPE_BLUETOOTH_SCO -> "bluetooth-sco"
            AudioDeviceInfo.TYPE_USB_DEVICE -> "usb-device"
            AudioDeviceInfo.TYPE_USB_HEADSET -> "usb-headset"
            AudioDeviceInfo.TYPE_HDMI -> "hdmi"
            else -> "type-${device.type}"
        }
        val name = device.productName?.toString()?.takeIf { it.isNotBlank() }
        return if (name == null) type else "$type:$name"
    }

    private fun currentAudioRouteSignature(): String {
        return audioManager
            ?.getDevices(AudioManager.GET_DEVICES_OUTPUTS)
            ?.filter { it.isSink }
            ?.map { "${it.id}:${it.type}:${it.productName}" }
            ?.sorted()
            ?.joinToString("|")
            .orEmpty()
    }

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
            inputMappingStore.profile(currentOverrideGameId, event.deviceDescriptor()),
        )
        if (mask == 0) {
            return super.dispatchKeyEvent(event)
        }

        when (event.action) {
            KeyEvent.ACTION_DOWN -> {
                rememberInputDevice(event)
                hardwareButtonKeys = hardwareButtonKeys or mask
                syncKeys(event.eventTime, "Key")
                return true
            }
            KeyEvent.ACTION_UP -> {
                hardwareButtonKeys = hardwareButtonKeys and mask.inv()
                syncKeys(event.eventTime, "Key")
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
        syncKeys(event.eventTime, "Motion")
        return true
    }

    private fun clearInput() {
        virtualKeys = 0
        hardwareButtonKeys = 0
        hardwareAxisKeys = 0
        gamepadView?.clearKeys()
        syncKeys(source = "Clear")
        if (fastForwardMode == FastForwardModes.ModeHold) {
            setFastForwardActive(false)
        }
        setRewindingActive(false)
    }

    private fun syncKeys(eventTimeMs: Long = SystemClock.uptimeMillis(), source: String = "Internal") {
        val keys = virtualKeys or hardwareButtonKeys or hardwareAxisKeys
        val filteredKeys = if (allowOpposingDirections) keys else filterOpposingDirections(keys)
        val nowMs = SystemClock.uptimeMillis()
        val startNs = SystemClock.elapsedRealtimeNanos()
        controller?.setKeys(filteredKeys)
        val durationUs = (SystemClock.elapsedRealtimeNanos() - startNs).coerceAtLeast(0L) / 1000L
        recordInputSync(source, eventTimeMs, nowMs, durationUs, filteredKeys)
    }

    private fun recordInputSync(source: String, eventTimeMs: Long, nowMs: Long, durationUs: Long, keys: Int) {
        lastInputSyncSource = source
        lastInputSyncEventAgeUs = (nowMs - eventTimeMs).coerceAtLeast(0L) * 1000L
        lastInputSyncDurationUs = durationUs
        maxInputSyncDurationUs = max(maxInputSyncDurationUs, durationUs)
        inputSyncSamples += 1
        if (durationUs > INPUT_SYNC_SLOW_THRESHOLD_US) {
            slowInputSyncSamples += 1
        }
        lastInputSyncKeys = keys
        lastInputSyncAtMs = SystemClock.elapsedRealtime()
    }

    private fun reloadPerGameOverridesFromStore() {
        loadPerGameOverridesFromStore()
        applyPerGameOverridesToRuntime()
    }

    private fun loadPerGameOverridesFromStore() {
        scaleMode = perGameOverrides.scaleMode(currentOverrideGameId, preferences.scaleMode)
        filterMode = perGameOverrides.filterMode(currentOverrideGameId, preferences.filterMode)
        interframeBlending = perGameOverrides.interframeBlending(currentOverrideGameId, preferences.interframeBlending)
        orientationMode = perGameOverrides.orientationMode(currentOverrideGameId, preferences.orientationMode)
        skipBios = perGameOverrides.skipBios(currentOverrideGameId, preferences.skipBios)
        frameSkip = perGameOverrides.frameSkip(currentOverrideGameId, preferences.frameSkip)
        muted = perGameOverrides.muted(currentOverrideGameId, preferences.muted)
        volumePercent = perGameOverrides.volumePercent(currentOverrideGameId, preferences.volumePercent)
        audioBufferMode = perGameOverrides.audioBufferMode(currentOverrideGameId, preferences.audioBufferMode)
        audioLowPassMode = perGameOverrides.audioLowPassMode(currentOverrideGameId, preferences.audioLowPassMode)
        fastForwardMode = perGameOverrides.fastForwardMode(currentOverrideGameId, preferences.fastForwardMode)
        fastForwardMultiplier = perGameOverrides.fastForwardMultiplier(
            currentOverrideGameId,
            preferences.fastForwardMultiplier,
        )
        rewindEnabled = perGameOverrides.rewindEnabled(currentOverrideGameId, preferences.rewindEnabled)
        rewindBufferCapacity = perGameOverrides.rewindBufferCapacity(
            currentOverrideGameId,
            preferences.rewindBufferCapacity,
        )
        rewindBufferInterval = perGameOverrides.rewindBufferInterval(
            currentOverrideGameId,
            preferences.rewindBufferInterval,
        )
        showVirtualGamepad = perGameOverrides.showVirtualGamepad(currentOverrideGameId, preferences.showVirtualGamepad)
        virtualGamepadSizePercent = perGameOverrides.virtualGamepadSizePercent(
            currentOverrideGameId,
            preferences.virtualGamepadSizePercent,
        )
        virtualGamepadOpacityPercent = perGameOverrides.virtualGamepadOpacityPercent(
            currentOverrideGameId,
            preferences.virtualGamepadOpacityPercent,
        )
        virtualGamepadSpacingPercent = perGameOverrides.virtualGamepadSpacingPercent(
            currentOverrideGameId,
            preferences.virtualGamepadSpacingPercent,
        )
        virtualGamepadHapticsEnabled = perGameOverrides.virtualGamepadHapticsEnabled(
            currentOverrideGameId,
            preferences.virtualGamepadHapticsEnabled,
        )
        virtualGamepadLeftHanded = perGameOverrides.virtualGamepadLeftHanded(
            currentOverrideGameId,
            preferences.virtualGamepadLeftHanded,
        )
        virtualGamepadLayoutOffsets = VirtualGamepadLayoutOffsets.parse(
            perGameOverrides.virtualGamepadLayout(currentOverrideGameId),
        )
        deadzonePercent = perGameOverrides.deadzonePercent(
            currentOverrideGameId,
            AndroidInputMapper.DefaultAxisThresholdPercent,
        )
        allowOpposingDirections = perGameOverrides.allowOpposingDirections(
            currentOverrideGameId,
            preferences.allowOpposingDirections,
        )
        rumbleEnabled = perGameOverrides.rumbleEnabled(currentOverrideGameId, preferences.rumbleEnabled)
        tiltEnabled = perGameOverrides.tiltEnabled(currentOverrideGameId, false)
        tiltOffsetX = perGameOverrides.tiltOffsetX(currentOverrideGameId, 0f)
        tiltOffsetY = perGameOverrides.tiltOffsetY(currentOverrideGameId, 0f)
        solarLevel = perGameOverrides.solarLevel(currentOverrideGameId, 255)
        useLightSensor = perGameOverrides.useLightSensor(currentOverrideGameId, false)
        cameraImagePath = perGameOverrides.cameraImagePath(currentOverrideGameId)
    }

    private fun applyPerGameOverridesToRuntime() {
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
        controller?.setSolarLevel(solarLevel)
        if (fastForwardMode == FastForwardModes.ModeHold && fastForward) {
            setFastForwardActive(false)
        }
        if (!rewindEnabled && rewinding) {
            setRewindingActive(false)
        }

        applyOrientationMode()
        applyGamepadStyle()
        gamepadView?.visibility = if (showVirtualGamepad) View.VISIBLE else View.GONE
        if (!showVirtualGamepad) {
            setGamepadLayoutEditing(false)
            gamepadView?.clearKeys()
        }
        hardwareButtonKeys = 0
        hardwareAxisKeys = 0
        syncKeys()
        applyPersistedCameraImage()
        updateSensorRegistration()
        updateRunButtons()
    }

    private fun saveScaleModePreference() {
        if (!perGameOverrides.setScaleMode(currentOverrideGameId, scaleMode)) {
            preferences.scaleMode = scaleMode
        }
    }

    private fun saveFilterModePreference() {
        if (!perGameOverrides.setFilterMode(currentOverrideGameId, filterMode)) {
            preferences.filterMode = filterMode
        }
    }

    private fun saveInterframeBlendingPreference() {
        if (!perGameOverrides.setInterframeBlending(currentOverrideGameId, interframeBlending)) {
            preferences.interframeBlending = interframeBlending
        }
    }

    private fun saveOrientationModePreference() {
        if (!perGameOverrides.setOrientationMode(currentOverrideGameId, orientationMode)) {
            preferences.orientationMode = orientationMode
        }
    }

    private fun saveSkipBiosPreference() {
        if (!perGameOverrides.setSkipBios(currentOverrideGameId, skipBios)) {
            preferences.skipBios = skipBios
        }
    }

    private fun saveMutedPreference() {
        if (!perGameOverrides.setMuted(currentOverrideGameId, muted)) {
            preferences.muted = muted
        }
    }

    private fun saveVolumePreference() {
        if (!perGameOverrides.setVolumePercent(currentOverrideGameId, volumePercent)) {
            preferences.volumePercent = volumePercent
        }
    }

    private fun saveAudioBufferPreference() {
        if (!perGameOverrides.setAudioBufferMode(currentOverrideGameId, audioBufferMode)) {
            preferences.audioBufferMode = audioBufferMode
        }
    }

    private fun saveAudioLowPassPreference() {
        if (!perGameOverrides.setAudioLowPassMode(currentOverrideGameId, audioLowPassMode)) {
            preferences.audioLowPassMode = audioLowPassMode
        }
    }

    private fun saveFastForwardModePreference() {
        if (!perGameOverrides.setFastForwardMode(currentOverrideGameId, fastForwardMode)) {
            preferences.fastForwardMode = fastForwardMode
        }
    }

    private fun saveFastForwardMultiplierPreference() {
        if (!perGameOverrides.setFastForwardMultiplier(currentOverrideGameId, fastForwardMultiplier)) {
            preferences.fastForwardMultiplier = fastForwardMultiplier
        }
    }

    private fun saveRewindPreference() {
        if (!perGameOverrides.setRewindEnabled(currentOverrideGameId, rewindEnabled)) {
            preferences.rewindEnabled = rewindEnabled
        }
        if (!perGameOverrides.setRewindBufferCapacity(currentOverrideGameId, rewindBufferCapacity)) {
            preferences.rewindBufferCapacity = rewindBufferCapacity
        }
        if (!perGameOverrides.setRewindBufferInterval(currentOverrideGameId, rewindBufferInterval)) {
            preferences.rewindBufferInterval = rewindBufferInterval
        }
    }

    private fun saveAutoStatePreference() {
        preferences.autoStateOnExit = autoStateOnExit
    }

    private fun saveGamepadPreference() {
        if (!perGameOverrides.setShowVirtualGamepad(currentOverrideGameId, showVirtualGamepad)) {
            preferences.showVirtualGamepad = showVirtualGamepad
        }
    }

    private fun saveGamepadStylePreference() {
        if (!perGameOverrides.setVirtualGamepadSizePercent(currentOverrideGameId, virtualGamepadSizePercent)) {
            preferences.virtualGamepadSizePercent = virtualGamepadSizePercent
        }
        if (!perGameOverrides.setVirtualGamepadOpacityPercent(currentOverrideGameId, virtualGamepadOpacityPercent)) {
            preferences.virtualGamepadOpacityPercent = virtualGamepadOpacityPercent
        }
        if (!perGameOverrides.setVirtualGamepadSpacingPercent(currentOverrideGameId, virtualGamepadSpacingPercent)) {
            preferences.virtualGamepadSpacingPercent = virtualGamepadSpacingPercent
        }
        if (!perGameOverrides.setVirtualGamepadHapticsEnabled(currentOverrideGameId, virtualGamepadHapticsEnabled)) {
            preferences.virtualGamepadHapticsEnabled = virtualGamepadHapticsEnabled
        }
        if (!perGameOverrides.setVirtualGamepadLeftHanded(currentOverrideGameId, virtualGamepadLeftHanded)) {
            preferences.virtualGamepadLeftHanded = virtualGamepadLeftHanded
        }
    }

    private fun saveGamepadLayoutPreference() {
        perGameOverrides.setVirtualGamepadLayout(currentOverrideGameId, virtualGamepadLayoutOffsets.serialize())
    }

    private fun saveFrameSkipPreference() {
        if (!perGameOverrides.setFrameSkip(currentOverrideGameId, frameSkip)) {
            preferences.frameSkip = frameSkip
        }
    }

    private fun saveDeadzonePreference() {
        perGameOverrides.setDeadzonePercent(currentOverrideGameId, deadzonePercent)
    }

    private fun saveOpposingDirectionsPreference() {
        if (!perGameOverrides.setAllowOpposingDirections(currentOverrideGameId, allowOpposingDirections)) {
            preferences.allowOpposingDirections = allowOpposingDirections
        }
    }

    private fun saveRumblePreference() {
        if (!perGameOverrides.setRumbleEnabled(currentOverrideGameId, rumbleEnabled)) {
            preferences.rumbleEnabled = rumbleEnabled
        }
    }

    private fun saveSensorPreference() {
        perGameOverrides.setTiltEnabled(currentOverrideGameId, tiltEnabled)
        perGameOverrides.setTiltCalibration(currentOverrideGameId, tiltOffsetX, tiltOffsetY)
        perGameOverrides.setSolarLevel(currentOverrideGameId, solarLevel)
        perGameOverrides.setUseLightSensor(currentOverrideGameId, useLightSensor)
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
            val runOptions = mutableListOf<Button>()
            val stateOptions = mutableListOf<Button>()
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
            runOptions.add(Button(context).apply {
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
            fastModeButton?.let(runOptions::add)
            fastMultiplierButton = Button(context).apply {
                setOnClickListener {
                    fastForwardMultiplier = FastForwardModes.nextMultiplier(fastForwardMultiplier)
                    controller?.setFastForwardMultiplier(fastForwardMultiplier)
                    saveFastForwardMultiplierPreference()
                    updateRunButtons()
                }
            }
            fastMultiplierButton?.let(runOptions::add)
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
            rewindEnabledButton?.let(runOptions::add)
            rewindBufferButton = Button(context).apply {
                setOnClickListener {
                    rewindBufferCapacity = RewindSettings.nextCapacity(rewindBufferCapacity)
                    setRewindingActive(false)
                    controller?.setRewindConfig(rewindEnabled, rewindBufferCapacity, rewindBufferInterval)
                    saveRewindPreference()
                    updateRunButtons()
                }
            }
            rewindBufferButton?.let(runOptions::add)
            rewindIntervalButton = Button(context).apply {
                setOnClickListener {
                    rewindBufferInterval = RewindSettings.nextInterval(rewindBufferInterval)
                    setRewindingActive(false)
                    controller?.setRewindConfig(rewindEnabled, rewindBufferCapacity, rewindBufferInterval)
                    saveRewindPreference()
                    updateRunButtons()
                }
            }
            rewindIntervalButton?.let(runOptions::add)
            autoStateButton = Button(context).apply {
                setOnClickListener {
                    autoStateOnExit = !autoStateOnExit
                    saveAutoStatePreference()
                    updateRunButtons()
                }
            }
            autoStateButton?.let(runOptions::add)
            frameSkipButton = Button(context).apply {
                setOnClickListener {
                    frameSkip = (frameSkip + 1) % FRAME_SKIP_LABELS.size
                    controller?.setFrameSkip(frameSkip)
                    saveFrameSkipPreference()
                    updateRunButtons()
                }
            }
            frameSkipButton?.let(runOptions::add)
            muteButton = Button(context).apply {
                setOnClickListener {
                    muted = !muted
                    controller?.setAudioEnabled(!muted)
                    saveMutedPreference()
                    updateRunButtons()
                }
            }
            muteButton?.let(runOptions::add)
            volumeButton = Button(context).apply {
                setOnClickListener {
                    val index = VOLUME_LEVELS.indexOf(volumePercent).takeIf { it >= 0 } ?: 0
                    volumePercent = VOLUME_LEVELS[(index + 1) % VOLUME_LEVELS.size]
                    controller?.setVolumePercent(volumePercent)
                    saveVolumePreference()
                    updateRunButtons()
                }
            }
            volumeButton?.let(runOptions::add)
            audioBufferButton = Button(context).apply {
                setOnClickListener {
                    audioBufferMode = (audioBufferMode + 1) % AudioBufferModes.labels.size
                    controller?.setAudioBufferSamples(AudioBufferModes.samplesFor(audioBufferMode))
                    saveAudioBufferPreference()
                    updateRunButtons()
                }
            }
            audioBufferButton?.let(runOptions::add)
            audioLowPassButton = Button(context).apply {
                setOnClickListener {
                    audioLowPassMode = (audioLowPassMode + 1) % AudioLowPassModes.labels.size
                    controller?.setLowPassRangePercent(AudioLowPassModes.rangeFor(audioLowPassMode))
                    saveAudioLowPassPreference()
                    updateRunButtons()
                }
            }
            audioLowPassButton?.let(runOptions::add)
            scaleButton = Button(context).apply {
                setOnClickListener {
                    scaleMode = (scaleMode + 1) % SCALE_LABELS.size
                    controller?.setScaleMode(scaleMode)
                    saveScaleModePreference()
                    updateVideoAspectRatio(videoAspectWidth, videoAspectHeight)
                    updateRunButtons()
                }
            }
            scaleButton?.let(runOptions::add)
            filterButton = Button(context).apply {
                setOnClickListener {
                    filterMode = (filterMode + 1) % FILTER_LABELS.size
                    controller?.setFilterMode(filterMode)
                    saveFilterModePreference()
                    updateRunButtons()
                }
            }
            filterButton?.let(runOptions::add)
            interframeBlendButton = Button(context).apply {
                setOnClickListener {
                    interframeBlending = !interframeBlending
                    controller?.setInterframeBlending(interframeBlending)
                    saveInterframeBlendingPreference()
                    updateRunButtons()
                }
            }
            interframeBlendButton?.let(runOptions::add)
            orientationButton = Button(context).apply {
                setOnClickListener {
                    orientationMode = (orientationMode + 1) % ORIENTATION_LABELS.size
                    applyOrientationMode()
                    saveOrientationModePreference()
                    updateRunButtons()
                }
            }
            orientationButton?.let(runOptions::add)
            skipBiosButton = Button(context).apply {
                setOnClickListener {
                    skipBios = !skipBios
                    controller?.setSkipBios(skipBios)
                    saveSkipBiosPreference()
                    updateRunButtons()
                    Toast.makeText(context, "BIOS setting applies on reset or next launch", Toast.LENGTH_SHORT).show()
                }
            }
            skipBiosButton?.let(runOptions::add)
            runOptions.add(Button(context).apply {
                text = "GameBIOS"
                setOnClickListener {
                    showGameBiosDialog()
                }
            })
            padButton = Button(context).apply {
                setOnClickListener {
                    showVirtualGamepad = !showVirtualGamepad
                    saveGamepadPreference()
                    gamepadView?.visibility = if (showVirtualGamepad) View.VISIBLE else View.GONE
                    if (!showVirtualGamepad) {
                        setGamepadLayoutEditing(false)
                        gamepadView?.clearKeys()
                    }
                    updateRunButtons()
                }
            }
            padButton?.let(runOptions::add)
            padSettingsButton = Button(context).apply {
                setOnClickListener {
                    showGamepadSettingsDialog()
                }
            }
            padSettingsButton?.let(runOptions::add)
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
            deadzoneButton?.let(runOptions::add)
            opposingDirectionsButton = Button(context).apply {
                setOnClickListener {
                    allowOpposingDirections = !allowOpposingDirections
                    saveOpposingDirectionsPreference()
                    syncKeys()
                    updateRunButtons()
                }
            }
            opposingDirectionsButton?.let(runOptions::add)
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
            rumbleButton?.let(runOptions::add)
            tiltButton = Button(context).apply {
                setOnClickListener {
                    tiltEnabled = !tiltEnabled
                    saveSensorPreference()
                    updateSensorRegistration()
                    updateRunButtons()
                    Toast.makeText(context, if (tiltEnabled) "Tilt enabled" else "Tilt disabled", Toast.LENGTH_SHORT).show()
                }
            }
            tiltButton?.let(runOptions::add)
            runOptions.add(Button(context).apply {
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
            solarButton?.let(runOptions::add)
            cameraButton = Button(context).apply {
                setOnClickListener {
                    showCameraImageDialog()
                }
            }
            cameraButton?.let(runOptions::add)
            runOptions.add(Button(context).apply {
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
            runOptions.add(Button(context).apply {
                text = "Diag"
                setOnClickListener {
                    exportRuntimeDiagnostics()
                }
            })
            runOptions.add(Button(context).apply {
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
            runOptions.add(Button(context).apply {
                text = "Export"
                setOnClickListener {
                    exportScreenshot()
                }
            })
            runRow.addView(Button(context).apply {
                text = "More"
                setOnClickListener {
                    showToolbarOptionsDialog("Run Options", runOptions)
                }
            })
            stateOptions.add(Button(context).apply {
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
            stateOptions.add(Button(context).apply {
                text = "Del"
                setOnClickListener {
                    deleteStateWithConfirmation()
                }
            })
            stateOptions.add(Button(context).apply {
                text = "StateOut"
                setOnClickListener {
                    openStateExportPicker()
                }
            })
            stateOptions.add(Button(context).apply {
                text = "StateIn"
                setOnClickListener {
                    importStateWithConfirmation()
                }
            })
            stateOptions.add(Button(context).apply {
                text = "Backup"
                setOnClickListener {
                    exportBatterySave()
                }
            })
            stateOptions.add(Button(context).apply {
                text = "DataOut"
                setOnClickListener {
                    openGameDataExportPicker()
                }
            })
            stateOptions.add(Button(context).apply {
                text = "DataIn"
                setOnClickListener {
                    openGameDataImportPicker()
                }
            })
            stateOptions.add(Button(context).apply {
                text = "Import"
                setOnClickListener {
                    openSaveImportPicker()
                }
            })
            stateOptions.add(Button(context).apply {
                text = "Cheats"
                setOnClickListener {
                    showCheatActionsDialog()
                }
            })
            stateOptions.add(Button(context).apply {
                text = "NoCheat"
                setOnClickListener {
                    clearCheatsWithConfirmation()
                }
            })
            stateOptions.add(Button(context).apply {
                text = "Patch"
                setOnClickListener {
                    openPatchImportPicker()
                }
            })
            stateOptions.add(Button(context).apply {
                text = "NoPatch"
                setOnClickListener {
                    clearPatchWithConfirmation()
                }
            })
            stateOptions.add(Button(context).apply {
                text = "Exit"
                setOnClickListener {
                    exitWithConfirmation()
                }
            })
            stateRow.addView(Button(context).apply {
                text = "More"
                setOnClickListener {
                    showToolbarOptionsDialog("State And Data", stateOptions)
                }
            })
            styleToolbarRow(runRow)
            styleToolbarRow(stateRow)
            addView(scrollableToolbarRow(runRow))
            addView(scrollableToolbarRow(stateRow))
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

    private fun scrollableToolbarRow(row: LinearLayout): HorizontalScrollView {
        return HorizontalScrollView(this).apply {
            isHorizontalScrollBarEnabled = false
            clipToPadding = false
            setPadding(dp(4), 0, dp(4), 0)
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT,
            )
            addView(row)
        }
    }

    private fun showToolbarOptionsDialog(title: String, options: List<Button>) {
        val content = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(12), dp(8), dp(12), dp(4))
        }
        val dialogButtons = mutableListOf<Pair<Button, Button>>()
        var row: LinearLayout? = null
        options.forEachIndexed { index, sourceButton ->
            if (index % 2 == 0) {
                row = LinearLayout(this).apply {
                    orientation = LinearLayout.HORIZONTAL
                    content.addView(this)
                }
            }
            val optionButton = Button(this).apply {
                setAllCaps(false)
                textSize = 13f
                maxLines = 1
                setOnClickListener {
                    sourceButton.performClick()
                    updateDialogOptionButtons(dialogButtons)
                }
            }
            dialogButtons.add(sourceButton to optionButton)
            row?.addView(
                optionButton,
                LinearLayout.LayoutParams(0, dp(44), 1f).apply {
                    leftMargin = dp(4)
                    rightMargin = dp(4)
                    bottomMargin = dp(8)
                },
            )
        }
        updateDialogOptionButtons(dialogButtons)
        AlertDialog.Builder(this)
            .setTitle(title)
            .setView(ScrollView(this).apply { addView(content) })
            .setPositiveButton("Close", null)
            .show()
    }

    private fun updateDialogOptionButtons(buttons: List<Pair<Button, Button>>) {
        buttons.forEach { (sourceButton, optionButton) ->
            optionButton.text = sourceButton.text
            optionButton.isEnabled = sourceButton.isEnabled
        }
    }

    private fun styleToolbarRow(row: LinearLayout) {
        row.clipToPadding = false
        row.setPadding(dp(4), 0, dp(4), 0)
        for (index in 0 until row.childCount) {
            (row.getChildAt(index) as? Button)?.applyToolbarStyle()
        }
    }

    private fun Button.applyToolbarStyle() {
        setAllCaps(false)
        setIncludeFontPadding(false)
        maxLines = 1
        textSize = 12f
        gravity = android.view.Gravity.CENTER
        minimumWidth = dp(48)
        minimumHeight = dp(38)
        setMinWidth(dp(48))
        setMinHeight(dp(38))
        setPadding(dp(10), 0, dp(10), 0)
        background?.mutate()?.alpha = 220
        layoutParams = LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.WRAP_CONTENT,
            dp(40),
        ).apply {
            rightMargin = dp(6)
            bottomMargin = dp(4)
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
        padSettingsButton?.text = if (gamepadLayoutEditing) "PadEdit*" else "PadCfg"
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

    private fun showGameBiosDialog() {
        val gameId = artifactGameId()
        if (gameId.isNullOrBlank()) {
            Toast.makeText(this, "Game BIOS unavailable", Toast.LENGTH_SHORT).show()
            return
        }
        val slots = BiosSlot.entries.toTypedArray()
        val labels = slots.map { slot ->
            val info = biosStore.infoForGame(gameId, slot)
            if (info == null) {
                "${slot.label}: Not set"
            } else {
                "${slot.label}: ${info.displayName} (${formatBytes(info.sizeBytes)})"
            }
        }.toTypedArray()
        AlertDialog.Builder(this)
            .setTitle("Game BIOS")
            .setItems(labels) { _, which ->
                showGameBiosSlotDialog(slots[which])
            }
            .setNegativeButton("Close", null)
            .show()
    }

    private fun showGameBiosSlotDialog(slot: BiosSlot) {
        val gameId = artifactGameId()
        if (gameId.isNullOrBlank()) {
            Toast.makeText(this, "Game BIOS unavailable", Toast.LENGTH_SHORT).show()
            return
        }
        val hasBios = biosStore.infoForGame(gameId, slot) != null
        val actions = if (hasBios) {
            arrayOf("Import or Replace", "Clear")
        } else {
            arrayOf("Import")
        }
        AlertDialog.Builder(this)
            .setTitle("${slot.label} BIOS")
            .setItems(actions) { _, which ->
                when (actions[which]) {
                    "Import", "Import or Replace" -> openGameBiosPicker(slot)
                    "Clear" -> clearGameBios(slot)
                }
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun openGameBiosPicker(slot: BiosSlot) {
        pendingGameBiosSlot = slot
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        startActivityForResult(intent, REQUEST_IMPORT_GAME_BIOS)
    }

    private fun importGameBios(uri: Uri) {
        val gameId = artifactGameId()
        if (gameId.isNullOrBlank()) {
            Toast.makeText(this, "Game BIOS unavailable", Toast.LENGTH_SHORT).show()
            return
        }
        val name = displayName(uri, "${pendingGameBiosSlot.label.lowercase(Locale.US)}.bios")
        val ok = biosStore.importForGame(gameId, pendingGameBiosSlot, uri, name)
        Toast.makeText(
            this,
            if (ok) "Game BIOS saved; applies on next launch" else "Game BIOS import failed",
            Toast.LENGTH_SHORT,
        ).show()
    }

    private fun clearGameBios(slot: BiosSlot) {
        val ok = biosStore.clearForGame(artifactGameId(), slot)
        Toast.makeText(
            this,
            if (ok) "Game BIOS cleared; applies on next launch" else "Game BIOS clear failed",
            Toast.LENGTH_SHORT,
        ).show()
    }

    private fun showCameraImageDialog() {
        val actions = if (cameraImagePath.isBlank()) {
            arrayOf("Capture Image", "Import Static Image")
        } else {
            arrayOf("Capture Image", "Import Static Image", "Clear Static Image")
        }
        AlertDialog.Builder(this)
            .setTitle("Game Boy Camera")
            .setItems(actions) { _, which ->
                when (actions[which]) {
                    "Capture Image" -> openCameraCapture()
                    "Import Static Image" -> openCameraImagePicker()
                    "Clear Static Image" -> clearCameraImage()
                }
            }
            .show()
    }

    private fun openCameraCapture() {
        val intent = Intent(MediaStore.ACTION_IMAGE_CAPTURE)
        runCatching {
            startActivityForResult(intent, REQUEST_CAPTURE_CAMERA_IMAGE)
        }.onFailure {
            Toast.makeText(this, "Camera app unavailable", Toast.LENGTH_SHORT).show()
        }
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
        val overrideGameId = currentOverrideGameId
        val storageGameId = artifactGameId()
        if (overrideGameId.isNullOrBlank() || storageGameId.isNullOrBlank()) {
            Toast.makeText(this, "Camera image unavailable for this game", Toast.LENGTH_SHORT).show()
            return
        }
        Thread {
            val path = copyCameraImage(storageGameId, imageUri)
            val appliedPath = path?.takeIf { setCameraImageFromPath(it) }
            if (appliedPath != null) {
                perGameOverrides.setCameraImagePath(overrideGameId, appliedPath)
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

    private fun importCapturedCameraImage(data: Intent?) {
        val bitmap = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            data?.extras?.getParcelable("data", Bitmap::class.java)
        } else {
            @Suppress("DEPRECATION")
            data?.extras?.get("data") as? Bitmap
        }
        if (bitmap == null) {
            Toast.makeText(this, "Camera capture failed", Toast.LENGTH_SHORT).show()
            return
        }
        val overrideGameId = currentOverrideGameId
        val storageGameId = artifactGameId()
        if (overrideGameId.isNullOrBlank() || storageGameId.isNullOrBlank()) {
            Toast.makeText(this, "Camera image unavailable for this game", Toast.LENGTH_SHORT).show()
            return
        }
        Thread {
            val path = copyCameraBitmap(storageGameId, bitmap)
            val appliedPath = path?.takeIf { setCameraImageFromPath(it) }
            if (appliedPath != null) {
                perGameOverrides.setCameraImagePath(overrideGameId, appliedPath)
            }
            runOnUiThread {
                if (appliedPath != null) {
                    cameraImagePath = appliedPath
                    updateRunButtons()
                    Toast.makeText(this, "Camera image captured", Toast.LENGTH_SHORT).show()
                } else {
                    Toast.makeText(this, "Camera capture import failed", Toast.LENGTH_SHORT).show()
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
            perGameOverrides.clearCameraImagePath(currentOverrideGameId)
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
            val target = cameraImageTarget(gameId)
            contentResolver.openInputStream(imageUri)?.use { input ->
                target.outputStream().use { output ->
                    input.copyTo(output)
                }
            } ?: return@runCatching null
            validatedCameraImagePath(target)
        }.getOrNull()
    }

    private fun copyCameraBitmap(gameId: String, bitmap: Bitmap): String? {
        return runCatching {
            val target = cameraImageTarget(gameId)
            target.outputStream().use { output ->
                if (!bitmap.compress(Bitmap.CompressFormat.PNG, 100, output)) {
                    return@runCatching null
                }
            }
            validatedCameraImagePath(target)
        }.getOrNull()
    }

    private fun importCameraImageFile(file: File): Boolean {
        val overrideGameId = currentOverrideGameId?.takeIf { it.isNotBlank() } ?: return false
        val storageGameId = artifactGameId()?.takeIf { it.isNotBlank() } ?: return false
        val targetPath = runCatching {
            val target = cameraImageTarget(storageGameId)
            file.copyTo(target, overwrite = true)
            validatedCameraImagePath(target)
        }.getOrNull() ?: return false
        perGameOverrides.setCameraImagePath(overrideGameId, targetPath)
        return true
    }

    private fun cameraImageTarget(gameId: String): File {
        val directory = File(filesDir, "camera-images")
        directory.mkdirs()
        return File(directory, "${sha1(gameId)}.image")
    }

    private fun validatedCameraImagePath(file: File): String? {
        val bounds = BitmapFactory.Options().apply {
            inJustDecodeBounds = true
        }
        BitmapFactory.decodeFile(file.absolutePath, bounds)
        return if (bounds.outWidth <= 0 || bounds.outHeight <= 0) {
            file.delete()
            null
        } else {
            file.absolutePath
        }
    }

    private fun clearCameraImage() {
        val previousPath = cameraImagePath
        perGameOverrides.clearCameraImagePath(currentOverrideGameId)
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
        lateinit var dialog: AlertDialog
        val editLayoutButton = Button(this).apply {
            text = if (gamepadLayoutEditing) "Stop Layout Edit" else "Edit Layout"
            setOnClickListener {
                setGamepadLayoutEditing(!gamepadLayoutEditing)
                dialog.dismiss()
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
            addView(editLayoutButton)
        }
        dialog = AlertDialog.Builder(this)
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
        virtualGamepadLayoutOffsets = VirtualGamepadLayoutOffsets()
        setGamepadLayoutEditing(false)
        applyGamepadStyle()
        saveGamepadStylePreference()
        saveGamepadLayoutPreference()
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
        gamepadView?.setLayoutOffsets(virtualGamepadLayoutOffsets)
    }

    private fun setGamepadLayoutEditing(enabled: Boolean) {
        gamepadLayoutEditing = enabled
        if (enabled && !showVirtualGamepad) {
            showVirtualGamepad = true
            saveGamepadPreference()
            gamepadView?.visibility = View.VISIBLE
        }
        gamepadView?.setLayoutEditMode(enabled)
        if (!enabled) {
            gamepadView?.clearKeys()
        }
        updateRunButtons()
    }

    private fun showInputMappingDialog() {
        val profile = inputMappingStore.profile(currentOverrideGameId, activeInputDeviceDescriptor)
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
        if (inputMappingStore.setKeyCode(currentOverrideGameId, activeInputDeviceDescriptor, mask, keyCode)) {
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
        inputMappingStore.reset(currentOverrideGameId, activeInputDeviceDescriptor)
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
            currentOverrideGameId,
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
            inputMappingStore.importProfileJson(currentOverrideGameId, activeInputDeviceDescriptor, json)
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
            "Device\nName: %s\nDescriptor: %s\n\nLast key\nAction: %s\nCode: %s\n\nLast axes\nSource: 0x%08X\nX: %.3f\nY: %.3f\nHat X: %.3f\nHat Y: %.3f\nLeft trigger: %.3f\nRight trigger: %.3f\nMapped keys: %s\nDeadzone: %d%%\n\nNative sync\nSource: %s\nMask: %s\nEvent age: %.3f ms\nCall: %.3f ms\nMax call: %.3f ms\nSamples: %d\nSlow samples: %d",
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
            lastInputSyncSource,
            formatGbaMask(lastInputSyncKeys),
            lastInputSyncEventAgeUs / 1000.0,
            lastInputSyncDurationUs / 1000.0,
            maxInputSyncDurationUs / 1000.0,
            inputSyncSamples,
            slowInputSyncSamples,
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
        return if (currentOverrideGameId == null) "Bindings apply globally." else "Bindings apply to this game."
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
        updateVideoAspectRatio(stats.videoWidth, stats.videoHeight)
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
            "FPS %.1f  Frame %.2fms\nPace %.2f/%.2fms  Jit %.2f  Late %.2f\nFrames %d\nROM %s  %s\nVideo %dx%d %s\nRun %s  Fast %s  Rewind %s/%d/%d  Skip %d\nAudio %s/%s  Vol %d%%  Buf %d  LPF %d  Und %d  Q %d\nScale %s  Filter %s  BIOS %s",
            fps,
            frameTimeMs,
            stats.frameTargetUs / 1000.0,
            stats.frameActualUs / 1000.0,
            stats.frameJitterUs / 1000.0,
            stats.frameLateUs / 1000.0,
            stats.frames,
            stats.romPlatform.ifBlank { "unknown" },
            stats.gameTitle.ifBlank { "untitled" },
            stats.videoWidth,
            stats.videoHeight,
            stats.videoPixelFormat,
            if (stats.running && !stats.paused) "on" else "off",
            if (stats.fastForward) FastForwardModes.labelForMultiplier(stats.fastForwardMultiplier) else "off",
            if (stats.rewinding) "on" else if (stats.rewindEnabled) "ready" else "off",
            stats.rewindBufferCapacity,
            stats.rewindBufferInterval,
            frameSkip,
            if (muted) "muted" else "on",
            stats.audioBackend,
            stats.volumePercent,
            stats.audioBufferSamples,
            stats.audioLowPassRange,
            stats.audioUnderruns,
            stats.audioEnqueuedBuffers,
            SCALE_LABELS.getOrElse(stats.scaleMode) { SCALE_LABELS[0] },
            FILTER_LABELS.getOrElse(stats.filterMode) { FILTER_LABELS[0] },
            if (stats.skipBios) "skip" else "boot",
        )
    }

    private fun updateVideoAspectRatio(width: Int, height: Int) {
        if (width <= 0 || height <= 0) {
            return
        }
        videoAspectWidth = width
        videoAspectHeight = height
        if (scaleMode == 1 || scaleMode == 4) {
            videoSurface?.setVideoAspectRatio(0, 0)
        } else {
            videoSurface?.setVideoAspectRatio(width, height)
        }
    }

    private fun exportRuntimeDiagnostics() {
        AppLogStore.append(this, runtimeDiagnosticsText())
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            openDiagnosticsExportPicker()
            return
        }
        Thread {
            val uri = LogExporter.exportRecent(this)
            runOnUiThread {
                if (uri == null) {
                    openDiagnosticsExportPicker()
                } else {
                    Toast.makeText(this, "Diagnostics exported", Toast.LENGTH_SHORT).show()
                }
            }
        }.start()
    }

    private fun openDiagnosticsExportPicker() {
        val intent = Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "text/plain"
            putExtra(Intent.EXTRA_TITLE, LogExporter.recentLogFileName())
            addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
        }
        startActivityForResult(intent, REQUEST_EXPORT_DIAGNOSTICS)
    }

    private fun exportDiagnosticsToUri(uri: Uri) {
        Thread {
            val ok = LogExporter.writeRecent(this, uri)
            runOnUiThread {
                Toast.makeText(
                    this,
                    if (ok) "Diagnostics exported" else "Diagnostics export failed",
                    Toast.LENGTH_SHORT,
                ).show()
            }
        }.start()
    }

    private fun runtimeDiagnosticsText(): String {
        val stats = controller?.stats()
        return buildString {
            appendLine("Runtime diagnostics")
            appendLine("gameId=$currentGameId")
            appendLine("stableGameId=$currentStableGameId")
            appendLine("overrideGameId=$currentOverrideGameId")
            EmulatorSession.currentGame()?.let { game ->
                if (game.launchStartedAtMs > 0L || game.loadedAtMs > 0L) {
                    appendLine("launchTiming loadStartedAtMs=${game.launchStartedAtMs} loadedAtMs=${game.loadedAtMs}")
                }
            }
            appendLine("paused=$userPaused fastForward=$fastForward rewinding=$rewinding")
            appendLine("stateSlot=$stateSlot autoStateOnExit=$autoStateOnExit")
            appendLine("video scale=${SCALE_LABELS.getOrElse(scaleMode) { SCALE_LABELS[0] }} filter=${FILTER_LABELS.getOrElse(filterMode) { FILTER_LABELS[0] }} interframe=$interframeBlending")
            appendLine("audio muted=$muted volume=$volumePercent buffer=${AudioBufferModes.nameFor(audioBufferMode)} lowPass=${AudioLowPassModes.nameFor(audioLowPassMode)}")
            appendLine("input virtual=$showVirtualGamepad deadzone=$deadzonePercent opposing=$allowOpposingDirections activeDevice=${activeInputDeviceName.orEmpty()}")
            appendLine("inputSync source=$lastInputSyncSource keys=${formatGbaMask(lastInputSyncKeys)} eventAgeUs=$lastInputSyncEventAgeUs nativeCallUs=$lastInputSyncDurationUs maxNativeCallUs=$maxInputSyncDurationUs samples=$inputSyncSamples slowSamples=$slowInputSyncSamples lastAtMs=$lastInputSyncAtMs")
            appendLine("sensors rumble=$rumbleEnabled tilt=$tiltEnabled solar=$solarLevel camera=${cameraImagePath.isNotBlank()}")
            if (stats == null) {
                appendLine("nativeStats=unavailable")
            } else {
                appendLine("nativeFrames=${stats.frames}")
                appendLine("nativeVideo=${stats.videoWidth}x${stats.videoHeight} format=${stats.videoPixelFormat}")
                appendLine("nativePacing targetUs=${stats.frameTargetUs} actualUs=${stats.frameActualUs} jitterUs=${stats.frameJitterUs} lateUs=${stats.frameLateUs} samples=${stats.framePacingSamples}")
                appendLine("nativeRun running=${stats.running} paused=${stats.paused} fast=${stats.fastForward} rewind=${stats.rewinding}")
                appendLine("nativeAudio backend=${stats.audioBackend} volume=${stats.volumePercent} buffer=${stats.audioBufferSamples} lowPass=${stats.audioLowPassRange} started=${stats.audioStarted} paused=${stats.audioPaused} enabled=${stats.audioEnabled} underruns=${stats.audioUnderruns} queuedBuffers=${stats.audioEnqueuedBuffers} queuedFrames=${stats.audioEnqueuedOutputFrames} readFrames=${stats.audioReadFrames} lastReadFrames=${stats.audioLastReadFrames}")
                appendLine("nativeInput current=${formatGbaMask(stats.inputKeys)} seen=${formatGbaMask(stats.seenInputKeys)}")
                appendLine("nativeRom platform=${stats.romPlatform} title=${stats.gameTitle} skipBios=${stats.skipBios}")
            }
        }
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

    private fun exportBatterySave() {
        val path = controller?.exportBatterySave()
        if (path == null) {
            Toast.makeText(this, "Export failed", Toast.LENGTH_SHORT).show()
            return
        }
        val uri = SaveExporter.exportToDocuments(this, path)
        if (uri != null) {
            Toast.makeText(this, "Save exported", Toast.LENGTH_SHORT).show()
        } else {
            openSaveExportPicker(path)
        }
    }

    private fun openSaveExportPicker(path: String) {
        pendingExportSavePath = path
        val fileName = File(path).name.ifBlank { "mgba-save.sav" }
        val intent = Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "application/octet-stream"
            putExtra(Intent.EXTRA_TITLE, fileName)
            addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
        }
        startActivityForResult(intent, REQUEST_EXPORT_SAVE)
    }

    private fun exportBatterySaveToUri(uri: Uri) {
        val path = pendingExportSavePath
        pendingExportSavePath = ""
        val ok = SaveExporter.writeToUri(this, path, uri)
        Toast.makeText(this, if (ok) "Save exported" else "Export failed", Toast.LENGTH_SHORT).show()
    }

    private fun openGameDataExportPicker() {
        val game = EmulatorSession.currentGame()
        if (game == null) {
            Toast.makeText(this, "No game data to export", Toast.LENGTH_SHORT).show()
            return
        }
        val base = game.displayName
            .substringBeforeLast('.')
            .replace(Regex("[^A-Za-z0-9._-]+"), "-")
            .trim('-')
            .ifBlank { "mgba-game-data" }
        val intent = Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "application/zip"
            putExtra(Intent.EXTRA_TITLE, "$base-mgba-data.zip")
            addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
        }
        startActivityForResult(intent, REQUEST_EXPORT_GAME_DATA)
    }

    private fun exportGameDataPackage(uri: Uri) {
        val resolver = contentResolver
        Thread {
            val ok = runCatching {
                resolver.openOutputStream(uri)?.use { output ->
                    ZipOutputStream(output.buffered()).use { zip ->
                        writeGameDataPackage(zip)
                    }
                } != null
            }.getOrDefault(false)
            runOnUiThread {
                Toast.makeText(this, if (ok) "Game data exported" else "Game data export failed", Toast.LENGTH_SHORT).show()
            }
        }.start()
    }

    private fun writeGameDataPackage(zip: ZipOutputStream) {
        val game = EmulatorSession.currentGame()
        val metadata = JSONObject()
            .put("version", 1)
            .put("exportedAt", System.currentTimeMillis())
            .put("displayName", game?.displayName.orEmpty())
            .put("uri", currentGameId.orEmpty())
            .put("stableId", currentStableGameId.orEmpty())
            .put("crc32", game?.crc32.orEmpty())
            .put("sha1", game?.sha1.orEmpty())
        zipText(zip, "metadata.json", metadata.toString(2))
        zipText(zip, "per-game-overrides.json", perGameOverrides.exportGameJson(currentOverrideGameId).toString(2))
        zipText(zip, "input-mappings.json", inputMappingStore.exportGameJson(currentOverrideGameId).toString(2))

        controller?.exportBatterySave()?.let { savePath ->
            zipFile(zip, "save/battery.sav", File(savePath))
        }
        cheatStore.fileForGame(cheatGameId())?.let { file ->
            zipFile(zip, "cheats/${file.name}", file)
        }
        patchStore.fileForGame(patchGameId())?.let { file ->
            zipFile(zip, "patches/${file.name}", file)
        }
        BiosSlot.entries.forEach { slot ->
            biosStore.fileForGame(artifactGameId(), slot)?.let { file ->
                zipFile(zip, "bios/${slot.name.lowercase(Locale.US)}.bios", file)
            }
        }
        cameraImagePath
            .takeIf { it.isNotBlank() }
            ?.let { File(it) }
            ?.let { file -> zipFile(zip, "camera/static.image", file) }
        for (slot in 1..9) {
            if (controller?.hasStateSlot(slot) == true) {
                exportStateSlotToTemp(slot)?.let { file ->
                    zipFile(zip, "states/slot-$slot.ss", file)
                    file.delete()
                }
            }
            stateThumbnailFile(slot)?.let { file ->
                zipFile(zip, "state-thumbnails/slot-$slot.png", file)
            }
        }
    }

    private fun exportStateSlotToTemp(slot: Int): File? {
        val directory = File(cacheDir, "game-data-export")
        directory.mkdirs()
        val file = File(directory, "slot-$slot-${SystemClock.elapsedRealtime()}.ss")
        val ok = runCatching {
            ParcelFileDescriptor.open(
                file,
                ParcelFileDescriptor.MODE_CREATE or ParcelFileDescriptor.MODE_TRUNCATE or ParcelFileDescriptor.MODE_READ_WRITE,
            ).use { descriptor ->
                controller?.exportStateSlotFd(slot, descriptor.fd) == true
            }
        }.getOrDefault(false)
        if (!ok) {
            file.delete()
        }
        return file.takeIf { ok && it.isFile }
    }

    private fun zipText(zip: ZipOutputStream, name: String, text: String) {
        zip.putNextEntry(ZipEntry(name))
        zip.write(text.toByteArray(Charsets.UTF_8))
        zip.closeEntry()
    }

    private fun zipFile(zip: ZipOutputStream, name: String, file: File) {
        if (!file.isFile) {
            return
        }
        zip.putNextEntry(ZipEntry(name))
        file.inputStream().use { input ->
            input.copyTo(zip)
        }
        zip.closeEntry()
    }

    private fun openGameDataImportPicker() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        startActivityForResult(intent, REQUEST_IMPORT_GAME_DATA)
    }

    private fun importGameDataPackage(uri: Uri) {
        val metadata = readGameDataMetadata(uri)
        if (metadata != null && !metadataMatchesCurrentGame(metadata)) {
            val exported = metadata.optString("displayName").ifBlank { "another game" }
            AlertDialog.Builder(this)
                .setTitle("Import data anyway?")
                .setMessage("This package appears to belong to $exported. Importing may overwrite current game data.")
                .setPositiveButton("Import") { _, _ -> importGameDataPackageConfirmed(uri) }
                .setNegativeButton("Cancel", null)
                .show()
        } else {
            importGameDataPackageConfirmed(uri)
        }
    }

    private fun readGameDataMetadata(uri: Uri): JSONObject? {
        return runCatching {
            contentResolver.openInputStream(uri)?.use { input ->
                ZipInputStream(input.buffered()).use { zip ->
                    while (true) {
                        val entry = zip.nextEntry ?: break
                        if (!entry.isDirectory && entry.name == "metadata.json") {
                            return@runCatching JSONObject(zip.readBytes().toString(Charsets.UTF_8))
                        }
                    }
                }
            }
            null
        }.getOrNull()
    }

    private fun metadataMatchesCurrentGame(metadata: JSONObject): Boolean {
        val exportedStableId = metadata.optString("stableId")
        val exportedCrc32 = metadata.optString("crc32")
        val exportedSha1 = metadata.optString("sha1")
        if (exportedStableId.isBlank() && exportedCrc32.isBlank() && exportedSha1.isBlank()) {
            return true
        }
        val currentGame = EmulatorSession.currentGame()
        return exportedStableId == currentStableGameId ||
            exportedSha1.equals(currentGame?.sha1.orEmpty(), ignoreCase = true) ||
            exportedCrc32.equals(currentGame?.crc32.orEmpty(), ignoreCase = true)
    }

    private fun importGameDataPackageConfirmed(uri: Uri) {
        val resolver = contentResolver
        Thread {
            val result = runCatching {
                resolver.openInputStream(uri)?.use { input ->
                    ZipInputStream(input.buffered()).use { zip ->
                        readGameDataPackageEntries(zip)
                    }
                } ?: GameDataImportResult()
            }.getOrDefault(GameDataImportResult())
            runOnUiThread {
                if (result.settingsImported || result.cameraImageImported) {
                    reloadPerGameOverridesFromStore()
                }
                if (result.inputMappingsImported) {
                    hardwareButtonKeys = 0
                    hardwareAxisKeys = 0
                    syncKeys()
                }
                val cheatsApplied = result.cheatsImported && applyStoredCheats()
                val patchApplied = result.patchImported && applyStoredPatch()
                updateStateThumbnail()
                Toast.makeText(
                    this,
                    result.toastMessage(cheatsApplied, patchApplied),
                    Toast.LENGTH_LONG,
                ).show()
            }
        }.start()
    }

    private fun readGameDataPackageEntries(zip: ZipInputStream): GameDataImportResult {
        val result = GameDataImportResult()
        val importDirectory = File(cacheDir, "game-data-import")
        importDirectory.mkdirs()
        while (true) {
            val entry = zip.nextEntry ?: break
            if (entry.isDirectory) {
                continue
            }
            val name = entry.name
            when {
                name == "per-game-overrides.json" -> {
                    result.settingsImported = perGameOverrides.importGameJson(
                        currentOverrideGameId,
                        JSONObject(zip.readBytes().toString(Charsets.UTF_8)),
                    ) || result.settingsImported
                }
                name == "input-mappings.json" -> {
                    result.inputMappingsImported = inputMappingStore.importGameJson(
                        currentOverrideGameId,
                        JSONObject(zip.readBytes().toString(Charsets.UTF_8)),
                    ) || result.inputMappingsImported
                }
                name == "save/battery.sav" -> {
                    val file = extractZipEntryToFile(zip, File(importDirectory, "battery.sav"))
                    result.saveImported = importBatterySaveFile(file) || result.saveImported
                    file.delete()
                }
                name.startsWith("states/slot-") && name.endsWith(".ss") -> {
                    val slot = name.substringAfter("slot-").substringBefore(".ss").toIntOrNull()
                    val file = extractZipEntryToFile(zip, File(importDirectory, "state-${slot ?: 0}.ss"))
                    if (slot != null && slot in 1..9) {
                        if (importStateSlotFile(file, slot)) {
                            result.statesImported += 1
                        }
                    }
                    file.delete()
                }
                name.startsWith("state-thumbnails/slot-") && name.endsWith(".png") -> {
                    val slot = name.substringAfter("slot-").substringBefore(".png").toIntOrNull()
                    if (slot != null && slot in 1..9) {
                        stateThumbnailFile(slot, forWrite = true)?.let { target ->
                            target.parentFile?.mkdirs()
                            extractZipEntryToFile(zip, target)
                            result.stateThumbnailsImported += 1
                        }
                    }
                }
                name.startsWith("cheats/") -> {
                    val fileName = name.substringAfterLast('/').takeIf { it.isNotBlank() }
                    if (fileName != null) {
                        val file = extractZipEntryToFile(zip, File(importDirectory, fileName))
                        result.cheatsImported = cheatStore.importForGameFile(
                            artifactGameId(),
                            file,
                            file.name,
                        ) || result.cheatsImported
                        file.delete()
                    }
                }
                name.startsWith("patches/") -> {
                    val fileName = name.substringAfterLast('/').takeIf { it.isNotBlank() }
                    if (fileName != null) {
                        val file = extractZipEntryToFile(zip, File(importDirectory, fileName))
                        result.patchImported = patchStore.importForGameFile(
                            artifactGameId(),
                            file,
                            file.name,
                        ) || result.patchImported
                        file.delete()
                    }
                }
                name.startsWith("bios/") -> {
                    val fileName = name.substringAfterLast('/').takeIf { it.isNotBlank() }
                    val slot = biosSlotForPackageEntry(name)
                    if (fileName != null && slot != null) {
                        val file = extractZipEntryToFile(zip, File(importDirectory, fileName))
                        if (biosStore.importForGameFile(artifactGameId(), slot, file, file.name)) {
                            result.biosImported += 1
                        }
                        file.delete()
                    }
                }
                name.startsWith("camera/") -> {
                    val fileName = name.substringAfterLast('/').takeIf { it.isNotBlank() }
                    if (fileName != null) {
                        val file = extractZipEntryToFile(zip, File(importDirectory, fileName))
                        result.cameraImageImported = importCameraImageFile(file) || result.cameraImageImported
                        file.delete()
                    }
                }
            }
        }
        return result
    }

    private fun extractZipEntryToFile(zip: ZipInputStream, file: File): File {
        file.parentFile?.mkdirs()
        file.outputStream().use { output ->
            zip.copyTo(output)
        }
        return file
    }

    private fun biosSlotForPackageEntry(name: String): BiosSlot? {
        val stem = name.substringAfter("bios/").substringAfterLast('/').substringBeforeLast('.')
        return BiosSlot.entries.firstOrNull { slot ->
            slot.name.equals(stem, ignoreCase = true) || slot.label.equals(stem, ignoreCase = true)
        }
    }

    private fun importBatterySaveFile(file: File): Boolean {
        return runCatching {
            ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY).use { descriptor ->
                controller?.importBatterySaveFd(descriptor.fd) == true
            }
        }.getOrDefault(false)
    }

    private fun importStateSlotFile(file: File, slot: Int): Boolean {
        return runCatching {
            ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY).use { descriptor ->
                controller?.importStateSlotFd(slot, descriptor.fd) == true
            }
        }.getOrDefault(false)
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

    private fun exportScreenshot() {
        val name = screenshotExportName()
        val uri = ScreenshotExporter.exportToPictures(this, name) { descriptor ->
            controller?.takeScreenshotFd(descriptor.fd) == true
        }
        if (uri != null) {
            Toast.makeText(this, "Screenshot exported", Toast.LENGTH_SHORT).show()
        } else {
            openScreenshotExportPicker(name)
        }
    }

    private fun openScreenshotExportPicker(name: String) {
        val intent = Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "image/png"
            putExtra(Intent.EXTRA_TITLE, name)
            addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
        }
        startActivityForResult(intent, REQUEST_EXPORT_SCREENSHOT)
    }

    private fun exportScreenshotToUri(uri: Uri) {
        val ok = ScreenshotExporter.writeToUri(this, uri) { descriptor ->
            controller?.takeScreenshotFd(descriptor.fd) == true
        }
        Toast.makeText(this, if (ok) "Screenshot exported" else "Screenshot export failed", Toast.LENGTH_SHORT).show()
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
                    which < entries.size -> showCheatEntryActionsDialog(which, entries[which])
                    which == entries.size -> showAddCheatDialog()
                    else -> openCheatImportPicker()
                }
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun showCheatEntryActionsDialog(index: Int, entry: CheatEntry) {
        val toggleLabel = if (entry.enabled) "Disable" else "Enable"
        AlertDialog.Builder(this)
            .setTitle(entry.name)
            .setItems(arrayOf(toggleLabel, "Edit", "Delete")) { _, which ->
                when (which) {
                    0 -> toggleCheat(index, !entry.enabled)
                    1 -> showEditCheatDialog(index, entry)
                    2 -> deleteCheat(index)
                }
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun showAddCheatDialog() {
        showCheatEditorDialog(
            title = "Add cheat",
            entry = CheatEntry(name = "", enabled = true, lines = emptyList()),
        ) { name, code ->
            val stored = cheatStore.addManual(cheatGameId(), name, code)
            val applied = stored && applyStoredCheats()
            val message = when {
                applied -> "Cheat saved"
                stored -> "Cheat saved; apply failed"
                else -> "Cheat save failed"
            }
            Toast.makeText(this, message, Toast.LENGTH_SHORT).show()
        }
    }

    private fun showEditCheatDialog(index: Int, entry: CheatEntry) {
        showCheatEditorDialog(
            title = "Edit cheat",
            entry = entry,
        ) { name, code ->
            val stored = cheatStore.updateEntry(cheatGameId(), index, name, code)
            val applied = stored && applyStoredCheats()
            val message = when {
                applied -> "Cheat updated"
                stored -> "Cheat saved; apply failed"
                else -> "Cheat update failed"
            }
            Toast.makeText(this, message, Toast.LENGTH_SHORT).show()
        }
    }

    private fun showCheatEditorDialog(
        title: String,
        entry: CheatEntry,
        onSave: (String, String) -> Unit,
    ) {
        val nameInput = EditText(this).apply {
            hint = "Name"
            setText(entry.name)
            setSingleLine(true)
            inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
        }
        val codeInput = EditText(this).apply {
            hint = "Code lines"
            setText(entry.lines.joinToString("\n"))
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
            .setTitle(title)
            .setView(body)
            .setPositiveButton("Save") { _, _ ->
                onSave(nameInput.text.toString(), codeInput.text.toString())
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

    private fun deleteCheat(index: Int) {
        val gameId = cheatGameId()
        val stored = cheatStore.removeEntry(gameId, index)
        val applied = stored && (applyStoredCheats() || cheatStore.entriesForGame(gameId).isEmpty())
        val message = when {
            applied -> "Cheat deleted"
            stored -> "Cheat deleted; apply failed"
            else -> "Cheat delete failed"
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

    private fun applyStoredPatch(): Boolean {
        val file = artifactGameIds()
            .asSequence()
            .mapNotNull { patchStore.fileForGame(it) }
            .firstOrNull()
            ?: return false
        return runCatching {
            ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY).use { descriptor ->
                controller?.importPatchFd(descriptor.fd) == true
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
        val applied = stored && applyStoredPatch()
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

    private fun formatBytes(bytes: Long): String {
        return when {
            bytes >= 1024L * 1024L -> String.format(Locale.US, "%.1f MiB", bytes / 1024.0 / 1024.0)
            bytes >= 1024L -> String.format(Locale.US, "%.1f KiB", bytes / 1024.0)
            else -> "$bytes B"
        }
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

    private class AspectRatioSurfaceView(context: android.content.Context) : SurfaceView(context) {
        private var videoWidth = 0
        private var videoHeight = 0

        fun setVideoAspectRatio(width: Int, height: Int) {
            val newWidth = width.coerceAtLeast(0)
            val newHeight = height.coerceAtLeast(0)
            if (videoWidth == newWidth && videoHeight == newHeight) {
                return
            }
            videoWidth = newWidth
            videoHeight = newHeight
            requestLayout()
        }

        override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
            val availableWidth = View.MeasureSpec.getSize(widthMeasureSpec)
            val availableHeight = View.MeasureSpec.getSize(heightMeasureSpec)
            if (availableWidth <= 0 || availableHeight <= 0 || videoWidth <= 0 || videoHeight <= 0) {
                setMeasuredDimension(availableWidth, availableHeight)
                return
            }

            val videoAspect = videoWidth.toFloat() / videoHeight.toFloat()
            val viewAspect = availableWidth.toFloat() / availableHeight.toFloat()
            if (viewAspect > videoAspect) {
                setMeasuredDimension((availableHeight * videoAspect).toInt(), availableHeight)
            } else {
                setMeasuredDimension(availableWidth, (availableWidth / videoAspect).toInt())
            }
        }
    }

    private data class CameraImagePixels(
        val pixels: IntArray,
        val width: Int,
        val height: Int,
    )

    private data class GameDataImportResult(
        var settingsImported: Boolean = false,
        var inputMappingsImported: Boolean = false,
        var saveImported: Boolean = false,
        var statesImported: Int = 0,
        var stateThumbnailsImported: Int = 0,
        var cheatsImported: Boolean = false,
        var patchImported: Boolean = false,
        var biosImported: Int = 0,
        var cameraImageImported: Boolean = false,
    ) {
        private val anyImported: Boolean
            get() = settingsImported ||
                inputMappingsImported ||
                saveImported ||
                statesImported > 0 ||
                stateThumbnailsImported > 0 ||
                cheatsImported ||
                patchImported ||
                biosImported > 0 ||
                cameraImageImported

        fun toastMessage(cheatsApplied: Boolean, patchApplied: Boolean): String {
            if (!anyImported) {
                return "Game data import failed"
            }
            val parts = buildList {
                if (saveImported) add("save")
                if (statesImported > 0) add("$statesImported states")
                if (stateThumbnailsImported > 0) add("$stateThumbnailsImported thumbnails")
                if (settingsImported) add("settings")
                if (inputMappingsImported) add("input")
                if (cheatsImported) add(if (cheatsApplied) "cheats" else "cheats saved")
                if (patchImported) add(if (patchApplied) "patch" else "patch saved")
                if (biosImported > 0) add("$biosImported BIOS")
                if (cameraImageImported) add("camera")
            }
            return "Game data imported: ${parts.joinToString(", ")}"
        }
    }

    companion object {
        private const val REQUEST_IMPORT_SAVE = 2001
        private const val REQUEST_IMPORT_CHEATS = 2002
        private const val REQUEST_EXPORT_STATE = 2003
        private const val REQUEST_IMPORT_STATE = 2004
        private const val REQUEST_EXPORT_INPUT_PROFILE = 2005
        private const val REQUEST_IMPORT_INPUT_PROFILE = 2006
        private const val REQUEST_IMPORT_PATCH = 2007
        private const val REQUEST_IMPORT_CAMERA_IMAGE = 2008
        private const val REQUEST_EXPORT_GAME_DATA = 2009
        private const val REQUEST_IMPORT_GAME_DATA = 2010
        private const val REQUEST_IMPORT_GAME_BIOS = 2011
        private const val REQUEST_EXPORT_DIAGNOSTICS = 2012
        private const val REQUEST_EXPORT_SAVE = 2013
        private const val REQUEST_EXPORT_SCREENSHOT = 2014
        private const val REQUEST_CAPTURE_CAMERA_IMAGE = 2015
        private const val FIRST_FRAME_POLL_MS = 16L
        private const val FIRST_FRAME_TIMEOUT_MS = 5000L
        private const val AUDIO_ROUTE_RESTART_DELAY_MS = 250L
        private const val INPUT_SYNC_SLOW_THRESHOLD_US = 2000L
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
