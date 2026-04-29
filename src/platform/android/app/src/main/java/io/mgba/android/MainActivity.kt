package io.mgba.android

import android.app.Activity
import android.content.Intent
import android.database.Cursor
import android.net.Uri
import android.os.Bundle
import android.provider.OpenableColumns
import android.view.Gravity
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import io.mgba.android.bridge.NativeBridge
import io.mgba.android.emulator.EmulatorSession

class MainActivity : Activity() {
    private lateinit var nativeStatus: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(24), dp(32), dp(24), dp(32))
            setBackgroundColor(getColor(R.color.mgba_background))
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT,
            )
        }

        val title = TextView(this).apply {
            text = getString(R.string.app_name)
            textSize = 34f
            setTextColor(getColor(R.color.mgba_text_primary))
        }

        val subtitle = TextView(this).apply {
            text = getString(R.string.app_subtitle)
            textSize = 16f
            setTextColor(getColor(R.color.mgba_text_secondary))
            setPadding(0, dp(6), 0, dp(28))
        }

        nativeStatus = TextView(this).apply {
            text = "${getString(R.string.native_version_label)}: ${NativeBridge.versionLabel()}"
            textSize = 15f
            setTextColor(getColor(R.color.mgba_text_primary))
            setPadding(0, 0, 0, dp(28))
        }

        val openButton = Button(this).apply {
            text = getString(R.string.open_emulator_placeholder)
            setOnClickListener {
                openRomPicker()
            }
        }

        root.addView(title)
        root.addView(subtitle)
        root.addView(nativeStatus)
        root.addView(openButton)
        setContentView(root)
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode != REQUEST_OPEN_ROM || resultCode != RESULT_OK) {
            return
        }

        val uri = data?.data ?: return
        runCatching {
            contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        val name = displayName(uri)
        val result = contentResolver.openFileDescriptor(uri, "r")?.use { descriptor ->
            val emulator = EmulatorSession.controller(this)
            emulator.loadRomFd(descriptor.fd, name)
        }
        nativeStatus.text = if (result?.ok == true) {
            "${getString(R.string.native_version_label)}: ${result.platform} ${result.title}"
        } else {
            "${getString(R.string.native_version_label)}: ${result?.message ?: "Unable to open ROM"}"
        }
        if (result?.ok == true) {
            startActivity(Intent(this, EmulatorActivity::class.java))
        }
    }

    private fun dp(value: Int): Int {
        return (value * resources.displayMetrics.density).toInt()
    }

    private fun openRomPicker() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            addFlags(Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION)
        }
        startActivityForResult(intent, REQUEST_OPEN_ROM)
    }

    private fun displayName(uri: Uri): String {
        var cursor: Cursor? = null
        return try {
            cursor = contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)
            if (cursor != null && cursor.moveToFirst()) {
                cursor.getString(0) ?: uri.lastPathSegment ?: "rom"
            } else {
                uri.lastPathSegment ?: "rom"
            }
        } finally {
            cursor?.close()
        }
    }

    companion object {
        private const val REQUEST_OPEN_ROM = 1001
    }
}
