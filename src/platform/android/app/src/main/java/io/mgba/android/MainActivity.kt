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
import android.widget.ScrollView
import android.widget.TextView
import io.mgba.android.bridge.NativeBridge
import io.mgba.android.emulator.EmulatorSession
import io.mgba.android.library.RomLibraryStore
import io.mgba.android.library.RomScanner
import io.mgba.android.library.RecentGameStore
import io.mgba.android.storage.BiosStore
import io.mgba.android.storage.PatchStore

class MainActivity : Activity() {
    private lateinit var nativeStatus: TextView
    private lateinit var recentStore: RecentGameStore
    private lateinit var libraryStore: RomLibraryStore
    private lateinit var biosStore: BiosStore
    private lateinit var patchStore: PatchStore
    private lateinit var biosButton: Button
    private lateinit var patchButton: Button
    private lateinit var recentContainer: LinearLayout
    private lateinit var libraryContainer: LinearLayout

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        recentStore = RecentGameStore(this)
        libraryStore = RomLibraryStore(this)
        biosStore = BiosStore(this)
        patchStore = PatchStore(this)

        val scroll = ScrollView(this).apply {
            setBackgroundColor(getColor(R.color.mgba_background))
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT,
            )
        }

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER_HORIZONTAL
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

        val scanButton = Button(this).apply {
            text = "Scan Folder"
            setOnClickListener {
                openFolderPicker()
            }
        }

        biosButton = Button(this).apply {
            text = biosStore.displayName?.let { "BIOS: $it" } ?: "Import BIOS"
            setOnClickListener {
                openBiosPicker()
            }
        }

        patchButton = Button(this).apply {
            text = patchStore.displayName?.let { "Patch: $it" } ?: "Import Patch"
            setOnClickListener {
                openPatchPicker()
            }
        }

        recentContainer = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(0, dp(24), 0, 0)
        }
        libraryContainer = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(0, dp(16), 0, 0)
        }

        root.addView(title)
        root.addView(subtitle)
        root.addView(nativeStatus)
        root.addView(openButton)
        root.addView(scanButton)
        root.addView(biosButton)
        root.addView(patchButton)
        root.addView(recentContainer)
        root.addView(libraryContainer)
        scroll.addView(root)
        setContentView(scroll)
        renderRecentGames()
        renderLibrary()
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (resultCode != RESULT_OK) {
            return
        }

        val uri = data?.data ?: return
        when (requestCode) {
            REQUEST_OPEN_ROM -> {
                runCatching {
                    contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
                }
                val name = displayName(uri)
                openRomUri(uri, name, shouldStoreRecent = true)
            }
            REQUEST_IMPORT_BIOS -> {
                val name = displayName(uri)
                val ok = biosStore.importDefault(uri, name)
                if (ok) {
                    biosButton.text = "BIOS: $name"
                }
                nativeStatus.text = "${getString(R.string.native_version_label)}: ${if (ok) "BIOS imported" else "BIOS import failed"}"
            }
            REQUEST_IMPORT_PATCH -> {
                val name = displayName(uri)
                val ok = patchStore.importDefault(uri, name)
                if (ok) {
                    patchButton.text = "Patch: $name"
                }
                nativeStatus.text = "${getString(R.string.native_version_label)}: ${if (ok) "Patch imported" else "Patch import failed"}"
            }
            REQUEST_SCAN_FOLDER -> {
                runCatching {
                    contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
                }
                val roms = runCatching { RomScanner(this).scan(uri) }.getOrDefault(emptyList())
                libraryStore.replace(roms)
                renderLibrary()
                nativeStatus.text = "${getString(R.string.native_version_label)}: ${roms.size} ROMs indexed"
            }
        }
    }

    private fun openRomUri(uri: Uri, name: String, shouldStoreRecent: Boolean) {
        val result = runCatching {
            contentResolver.openFileDescriptor(uri, "r")?.use { descriptor ->
                val emulator = EmulatorSession.controller(this)
                emulator.loadRomFd(descriptor.fd, name)
            }
        }.getOrNull()
        nativeStatus.text = if (result?.ok == true) {
            "${getString(R.string.native_version_label)}: ${result.platform} ${result.title}"
        } else {
            "${getString(R.string.native_version_label)}: ${result?.message ?: "Unable to open ROM"}"
        }
        if (result?.ok == true) {
            if (shouldStoreRecent) {
                recentStore.add(uri, name)
                renderRecentGames()
            }
            startActivity(Intent(this, EmulatorActivity::class.java))
        }
    }

    private fun renderRecentGames() {
        recentContainer.removeAllViews()
        val recentGames = recentStore.list()
        if (recentGames.isEmpty()) {
            return
        }

        recentContainer.addView(TextView(this).apply {
            text = "Recent"
            textSize = 14f
            setTextColor(getColor(R.color.mgba_text_secondary))
            setPadding(0, 0, 0, dp(8))
        })
        recentGames.forEach { game ->
            recentContainer.addView(Button(this).apply {
                text = game.displayName
                setOnClickListener {
                    openRomUri(game.uri, game.displayName, shouldStoreRecent = true)
                }
            })
        }
    }

    private fun renderLibrary() {
        libraryContainer.removeAllViews()
        val roms = libraryStore.list()
        if (roms.isEmpty()) {
            return
        }

        libraryContainer.addView(TextView(this).apply {
            text = "Library"
            textSize = 14f
            setTextColor(getColor(R.color.mgba_text_secondary))
            setPadding(0, 0, 0, dp(8))
        })
        roms.take(MAX_LIBRARY_ITEMS).forEach { rom ->
            libraryContainer.addView(Button(this).apply {
                text = rom.displayName
                setOnClickListener {
                    openRomUri(rom.uri, rom.displayName, shouldStoreRecent = true)
                }
            })
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

    private fun openFolderPicker() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT_TREE).apply {
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            addFlags(Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION)
        }
        startActivityForResult(intent, REQUEST_SCAN_FOLDER)
    }

    private fun openBiosPicker() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        startActivityForResult(intent, REQUEST_IMPORT_BIOS)
    }

    private fun openPatchPicker() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        startActivityForResult(intent, REQUEST_IMPORT_PATCH)
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
        private const val REQUEST_IMPORT_BIOS = 1002
        private const val REQUEST_IMPORT_PATCH = 1003
        private const val REQUEST_SCAN_FOLDER = 1004
        private const val MAX_LIBRARY_ITEMS = 24
    }
}
