package io.mgba.android

import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.graphics.BitmapFactory
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.view.WindowInsets
import android.view.WindowInsetsController
import android.view.WindowManager
import android.widget.Button
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import io.mgba.android.emulator.EmulatorController
import io.mgba.android.emulator.EmulatorSession
import io.mgba.android.input.AndroidInputMapper
import io.mgba.android.input.GbaButtons
import io.mgba.android.input.VirtualGamepadView
import io.mgba.android.library.RomLibraryStore
import io.mgba.android.settings.EmulatorPreferences
import io.mgba.android.settings.InputMappingStore
import io.mgba.android.settings.PerGameOverrideStore
import io.mgba.android.storage.ScreenshotExporter
import io.mgba.android.storage.ScreenshotShareProvider
import io.mgba.android.storage.SaveExporter
import java.io.File
import java.security.MessageDigest
import java.util.Locale

class EmulatorActivity : Activity(), SurfaceHolder.Callback {
    private var controller: EmulatorController? = null
    private var gamepadView: VirtualGamepadView? = null
    private lateinit var preferences: EmulatorPreferences
    private lateinit var perGameOverrides: PerGameOverrideStore
    private lateinit var inputMappingStore: InputMappingStore
    private var currentGameId: String? = null
    private var activeInputDeviceDescriptor: String? = null
    private var activeInputDeviceName: String? = null
    private var virtualKeys = 0
    private var hardwareButtonKeys = 0
    private var hardwareAxisKeys = 0
    private var stateSlot = 1
    private var slotButton: Button? = null
    private var stateThumbnailView: ImageView? = null
    private var pauseButton: Button? = null
    private var fastButton: Button? = null
    private var muteButton: Button? = null
    private var scaleButton: Button? = null
    private var padButton: Button? = null
    private var statsButton: Button? = null
    private var statsOverlay: TextView? = null
    private var userPaused = false
    private var fastForward = false
    private var muted = false
    private var showVirtualGamepad = true
    private var showStats = false
    private var lastStatsFrames = 0L
    private var lastStatsAtMs = 0L
    private var pendingExportStateSlot = 1
    private var pendingImportStateSlot = 1
    private var pendingHardwareMappingMask = 0
    private var playAccountingStartedAtMs = 0L
    private var scaleMode = 0
    private var hasSurface = false
    private var inputMappingDialog: AlertDialog? = null
    private var keyCaptureDialog: AlertDialog? = null
    private val statsHandler = Handler(Looper.getMainLooper())
    private val statsRunnable = object : Runnable {
        override fun run() {
            updateStatsOverlay()
            if (showStats) {
                statsHandler.postDelayed(this, 1000L)
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        preferences = EmulatorPreferences(this)
        perGameOverrides = PerGameOverrideStore(this)
        inputMappingStore = InputMappingStore(this)
        currentGameId = EmulatorSession.currentGame()?.uri
        scaleMode = perGameOverrides.scaleMode(currentGameId, preferences.scaleMode)
        muted = perGameOverrides.muted(currentGameId, preferences.muted)
        showVirtualGamepad = perGameOverrides.showVirtualGamepad(currentGameId, preferences.showVirtualGamepad)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        enterImmersiveMode()
        controller = EmulatorSession.current()
        if (controller == null) {
            finish()
            return
        }
        controller?.setScaleMode(scaleMode)
        controller?.setAudioEnabled(!muted)

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
        }
    }

    override fun onPause() {
        clearInput()
        recordPlayTime()
        stopStatsOverlay()
        controller?.pause()
        super.onPause()
    }

    override fun onDestroy() {
        clearInput()
        recordPlayTime()
        stopStatsOverlay()
        controller?.setSurface(null)
        super.onDestroy()
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        hasSurface = true
        controller?.setSurface(holder.surface)
        controller?.start()
        if (userPaused) {
            controller?.pause()
        } else {
            startPlayAccounting()
        }
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        controller?.setSurface(holder.surface)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        hasSurface = false
        clearInput()
        recordPlayTime()
        controller?.pause()
        controller?.setSurface(null)
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
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

    private fun saveScaleModePreference() {
        if (!perGameOverrides.setScaleMode(currentGameId, scaleMode)) {
            preferences.scaleMode = scaleMode
        }
    }

    private fun saveMutedPreference() {
        if (!perGameOverrides.setMuted(currentGameId, muted)) {
            preferences.muted = muted
        }
    }

    private fun saveGamepadPreference() {
        if (!perGameOverrides.setShowVirtualGamepad(currentGameId, showVirtualGamepad)) {
            preferences.showVirtualGamepad = showVirtualGamepad
        }
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
                        controller?.pause()
                    } else {
                        controller?.resume()
                        startPlayAccounting()
                    }
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
            fastButton = Button(context).apply {
                setOnClickListener {
                    fastForward = !fastForward
                    controller?.setFastForward(fastForward)
                    updateRunButtons()
                }
            }
            runRow.addView(fastButton)
            muteButton = Button(context).apply {
                setOnClickListener {
                    muted = !muted
                    controller?.setAudioEnabled(!muted)
                    saveMutedPreference()
                    updateRunButtons()
                }
            }
            runRow.addView(muteButton)
            scaleButton = Button(context).apply {
                setOnClickListener {
                    scaleMode = (scaleMode + 1) % SCALE_LABELS.size
                    controller?.setScaleMode(scaleMode)
                    saveScaleModePreference()
                    updateRunButtons()
                }
            }
            runRow.addView(scaleButton)
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
                    val path = controller?.takeScreenshot()
                    if (path != null) {
                        recordScreenshotCover(path)
                    }
                    val uri = path?.let { ScreenshotExporter.exportToPictures(context, it) }
                    Toast.makeText(
                        context,
                        if (uri != null) "Screenshot exported" else "Export unavailable",
                        Toast.LENGTH_SHORT,
                    ).show()
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
                    openCheatImportPicker()
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

    private fun updateRunButtons() {
        pauseButton?.text = if (userPaused) "Resume" else "Pause"
        fastButton?.text = if (fastForward) "1x" else "Fast"
        muteButton?.text = if (muted) "Sound" else "Mute"
        scaleButton?.text = SCALE_LABELS[scaleMode]
        padButton?.text = if (showVirtualGamepad) "Pad" else "No Pad"
        statsButton?.text = if (showStats) "Stats*" else "Stats"
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

    private fun rememberInputDevice(event: KeyEvent) {
        val descriptor = event.deviceDescriptor() ?: return
        activeInputDeviceDescriptor = descriptor
        activeInputDeviceName = event.device?.name
    }

    private fun KeyEvent.deviceDescriptor(): String? {
        return device?.descriptor?.takeIf { it.isNotBlank() }
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
        val fps = if (lastStatsAtMs > 0L && now > lastStatsAtMs) {
            (stats.frames - lastStatsFrames).coerceAtLeast(0L) * 1000.0 / (now - lastStatsAtMs)
        } else {
            0.0
        }
        lastStatsFrames = stats.frames
        lastStatsAtMs = now
        statsOverlay?.text = String.format(
            Locale.US,
            "FPS %.1f\nFrames %d\nVideo %dx%d\nRun %s  Fast %s",
            fps,
            stats.frames,
            stats.videoWidth,
            stats.videoHeight,
            if (stats.running && !stats.paused) "on" else "off",
            if (stats.fastForward) "on" else "off",
        )
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
        val target = stateThumbnailFile(slot) ?: return false
        return runCatching {
            target.parentFile?.mkdirs()
            File(sourcePath).copyTo(target, overwrite = true)
            File(sourcePath).delete()
            true
        }.getOrDefault(false)
    }

    private fun deleteStateThumbnail(slot: Int) {
        stateThumbnailFile(slot)?.delete()
    }

    private fun stateThumbnailFile(slot: Int): File? {
        val gameId = currentGameId ?: return null
        return File(File(filesDir, "state-thumbnails"), "${sha1(gameId)}-slot-$slot.bmp")
    }

    private fun sha1(value: String): String {
        val bytes = MessageDigest.getInstance("SHA-1").digest(value.toByteArray(Charsets.UTF_8))
        return bytes.joinToString("") { "%02x".format(it.toInt() and 0xff) }
    }

    private fun shareScreenshot(path: String) {
        val uri = ScreenshotShareProvider.uriFor(this, path)
        val intent = Intent(Intent.ACTION_SEND).apply {
            type = "image/bmp"
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

    private fun importBatterySave(uri: Uri) {
        val ok = runCatching {
            contentResolver.openFileDescriptor(uri, "r")?.use { descriptor ->
                controller?.importBatterySaveFd(descriptor.fd) == true
            } == true
        }.getOrDefault(false)
        Toast.makeText(this, if (ok) "Save imported" else "Import failed", Toast.LENGTH_SHORT).show()
    }

    private fun importCheats(uri: Uri) {
        val ok = runCatching {
            contentResolver.openFileDescriptor(uri, "r")?.use { descriptor ->
                controller?.importCheatsFd(descriptor.fd) == true
            } == true
        }.getOrDefault(false)
        Toast.makeText(this, if (ok) "Cheats imported" else "Cheat import failed", Toast.LENGTH_SHORT).show()
    }

    private fun dp(value: Int): Int {
        return (value * resources.displayMetrics.density).toInt()
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

    companion object {
        private const val REQUEST_IMPORT_SAVE = 2001
        private const val REQUEST_IMPORT_CHEATS = 2002
        private const val REQUEST_EXPORT_STATE = 2003
        private const val REQUEST_IMPORT_STATE = 2004
        private val SCALE_LABELS = arrayOf("Fit", "Fill", "Int")
    }
}
