package io.mgba.android

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
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
import android.widget.LinearLayout
import android.widget.Toast
import io.mgba.android.emulator.EmulatorController
import io.mgba.android.emulator.EmulatorSession
import io.mgba.android.input.AndroidInputMapper
import io.mgba.android.input.VirtualGamepadView
import io.mgba.android.storage.ScreenshotExporter
import io.mgba.android.storage.ScreenshotShareProvider
import io.mgba.android.storage.SaveExporter

class EmulatorActivity : Activity(), SurfaceHolder.Callback {
    private var controller: EmulatorController? = null
    private var gamepadView: VirtualGamepadView? = null
    private var virtualKeys = 0
    private var hardwareButtonKeys = 0
    private var hardwareAxisKeys = 0
    private var stateSlot = 1
    private var slotButton: Button? = null
    private var pauseButton: Button? = null
    private var fastButton: Button? = null
    private var userPaused = false
    private var fastForward = false
    private var hasSurface = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        enterImmersiveMode()
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

        setContentView(root)
    }

    override fun onResume() {
        super.onResume()
        enterImmersiveMode()
        if (hasSurface && !userPaused) {
            controller?.resume()
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
        if (userPaused) {
            controller?.pause()
        }
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
                        controller?.pause()
                    } else {
                        controller?.resume()
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
            runRow.addView(Button(context).apply {
                text = "Shot"
                setOnClickListener {
                    val path = controller?.takeScreenshot()
                    if (path == null) {
                        Toast.makeText(context, "Screenshot failed", Toast.LENGTH_SHORT).show()
                    } else {
                        shareScreenshot(path)
                    }
                }
            })
            runRow.addView(Button(context).apply {
                text = "Export"
                setOnClickListener {
                    val path = controller?.takeScreenshot()
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
                    val ok = controller?.saveStateSlot(stateSlot) == true
                    Toast.makeText(context, if (ok) "State saved" else "Save failed", Toast.LENGTH_SHORT).show()
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
            updateSlotButton()
            updateRunButtons()
        }
    }

    private fun updateSlotButton() {
        slotButton?.text = "Slot $stateSlot"
    }

    private fun updateRunButtons() {
        pauseButton?.text = if (userPaused) "Resume" else "Pause"
        fastButton?.text = if (fastForward) "1x" else "Fast"
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
    }
}
