package io.mgba.android

import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.database.Cursor
import android.net.Uri
import android.os.Bundle
import android.provider.OpenableColumns
import android.text.Editable
import android.text.InputType
import android.text.TextWatcher
import android.text.format.DateUtils
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import io.mgba.android.bridge.NativeBridge
import io.mgba.android.emulator.EmulatorSession
import io.mgba.android.library.LibraryRom
import io.mgba.android.library.RomLibraryStore
import io.mgba.android.library.RomScanner
import io.mgba.android.library.RecentGameStore
import io.mgba.android.storage.BiosStore
import io.mgba.android.storage.LogExporter
import io.mgba.android.storage.PatchStore

class MainActivity : Activity() {
    private lateinit var nativeStatus: TextView
    private lateinit var recentStore: RecentGameStore
    private lateinit var libraryStore: RomLibraryStore
    private lateinit var biosStore: BiosStore
    private lateinit var patchStore: PatchStore
    private lateinit var scanButton: Button
    private lateinit var biosButton: Button
    private lateinit var patchButton: Button
    private lateinit var recentContainer: LinearLayout
    private lateinit var librarySearch: EditText
    private lateinit var libraryFilterButton: Button
    private lateinit var libraryContainer: LinearLayout
    private var libraryFilter = ""
    private var libraryMode = LibraryMode.All
    private var scanThread: Thread? = null
    @Volatile
    private var scanGeneration = 0

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

        scanButton = Button(this).apply {
            text = "Scan Folder"
            setOnClickListener {
                if (scanThread?.isAlive == true) {
                    cancelLibraryScan()
                } else {
                    openFolderPicker()
                }
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

        val aboutButton = Button(this).apply {
            text = "About"
            setOnClickListener {
                showAboutDialog()
            }
        }

        val logButton = Button(this).apply {
            text = "Export Logs"
            setOnClickListener {
                exportLogs()
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
        librarySearch = EditText(this).apply {
            hint = "Search Library"
            inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
            setSingleLine(true)
            setTextColor(getColor(R.color.mgba_text_primary))
            setHintTextColor(getColor(R.color.mgba_text_secondary))
            visibility = View.GONE
            addTextChangedListener(object : TextWatcher {
                override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) = Unit

                override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {
                    libraryFilter = s?.toString().orEmpty()
                    renderLibrary()
                }

                override fun afterTextChanged(s: Editable?) = Unit
            })
        }
        libraryFilterButton = Button(this).apply {
            visibility = View.GONE
            setOnClickListener {
                libraryMode = libraryMode.next()
                renderLibrary()
            }
        }

        root.addView(title)
        root.addView(subtitle)
        root.addView(nativeStatus)
        root.addView(openButton)
        root.addView(scanButton)
        root.addView(biosButton)
        root.addView(patchButton)
        root.addView(aboutButton)
        root.addView(logButton)
        root.addView(recentContainer)
        root.addView(librarySearch)
        root.addView(libraryFilterButton)
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
                scanLibraryInBackground(uri)
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
            EmulatorSession.setCurrentGame(uri.toString(), name)
            if (shouldStoreRecent) {
                recentStore.add(uri, name)
                renderRecentGames()
            }
            libraryStore.markPlayed(uri)
            renderLibrary()
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
        val allRoms = libraryStore.list()
        librarySearch.visibility = if (allRoms.isEmpty()) View.GONE else View.VISIBLE
        libraryFilterButton.visibility = if (allRoms.isEmpty()) View.GONE else View.VISIBLE
        libraryFilterButton.text = "Filter: ${libraryMode.label}"
        if (allRoms.isEmpty()) {
            return
        }

        val query = libraryFilter.trim()
        val modeRoms = allRoms.filter { libraryMode.matches(it) }
        val roms = if (query.isEmpty()) {
            modeRoms
        } else {
            modeRoms.filter {
                it.displayName.contains(query, ignoreCase = true) ||
                    it.title.contains(query, ignoreCase = true) ||
                    it.platform.contains(query, ignoreCase = true) ||
                    it.crc32.contains(query, ignoreCase = true) ||
                    it.sha1.contains(query, ignoreCase = true)
            }
        }

        libraryContainer.addView(TextView(this).apply {
            text = if (query.isEmpty() && libraryMode == LibraryMode.All) {
                "Library (${allRoms.size})"
            } else {
                "Library (${roms.size}/${allRoms.size})"
            }
            textSize = 14f
            setTextColor(getColor(R.color.mgba_text_secondary))
            setPadding(0, 0, 0, dp(8))
        })
        if (roms.isEmpty()) {
            libraryContainer.addView(TextView(this).apply {
                text = "No matches"
                textSize = 14f
                setTextColor(getColor(R.color.mgba_text_secondary))
                setPadding(0, 0, 0, dp(8))
            })
            return
        }
        roms.take(MAX_LIBRARY_ITEMS).forEach { rom ->
            libraryContainer.addView(LinearLayout(this).apply {
                orientation = LinearLayout.HORIZONTAL
                addView(Button(context).apply {
                    text = libraryButtonLabel(rom)
                    setOnClickListener {
                        openRomUri(rom.uri, rom.displayName, shouldStoreRecent = true)
                    }
                    layoutParams = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f)
                })
                addView(Button(context).apply {
                    text = if (rom.favorite) "Fav*" else "Fav"
                    setOnClickListener {
                        libraryStore.toggleFavorite(rom.uri)
                        renderLibrary()
                    }
                })
                addView(Button(context).apply {
                    text = "Del"
                    setOnClickListener {
                        confirmRemoveLibraryRom(rom)
                    }
                })
            })
        }
        if (roms.size > MAX_LIBRARY_ITEMS) {
            libraryContainer.addView(TextView(this).apply {
                text = "Showing $MAX_LIBRARY_ITEMS of ${roms.size}"
                textSize = 13f
                setTextColor(getColor(R.color.mgba_text_secondary))
                setPadding(0, dp(6), 0, 0)
            })
        }
    }

    private fun libraryButtonLabel(rom: LibraryRom): String {
        if (rom.lastPlayedAt <= 0L) {
            return libraryTitleLine(rom)
        }
        val relative = DateUtils.getRelativeTimeSpanString(
            rom.lastPlayedAt,
            System.currentTimeMillis(),
            DateUtils.MINUTE_IN_MILLIS,
        )
        return "${libraryTitleLine(rom)}\nLast played $relative"
    }

    private fun libraryTitleLine(rom: LibraryRom): String {
        val marker = if (rom.favorite) "[*] " else ""
        val title = rom.title.ifBlank { rom.displayName }
        val platform = rom.platform.takeIf { it.isNotBlank() }?.let { " [$it]" }.orEmpty()
        val size = rom.fileSize.takeIf { it > 0L }?.let { " ${formatBytes(it)}" }.orEmpty()
        return "$marker$title$platform$size"
    }

    private fun formatBytes(bytes: Long): String {
        val mib = bytes / (1024.0 * 1024.0)
        return String.format(java.util.Locale.US, "%.1f MiB", mib)
    }

    private fun confirmRemoveLibraryRom(rom: LibraryRom) {
        AlertDialog.Builder(this)
            .setTitle("Remove from library?")
            .setMessage(rom.displayName)
            .setPositiveButton("Remove") { _, _ ->
                libraryStore.remove(rom.uri)
                renderLibrary()
                nativeStatus.text = "${getString(R.string.native_version_label)}: Removed from library"
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun scanLibraryInBackground(uri: Uri) {
        val generation = ++scanGeneration
        scanThread?.interrupt()
        scanButton.text = "Cancel Scan"
        nativeStatus.text = "${getString(R.string.native_version_label)}: Scanning folder"
        val thread = Thread {
            val result = runCatching { RomScanner(this).scan(uri) }
            runOnUiThread {
                if (generation != scanGeneration) {
                    return@runOnUiThread
                }
                scanThread = null
                scanButton.text = "Scan Folder"
                result
                    .onSuccess { roms ->
                        libraryStore.replace(roms)
                        renderLibrary()
                        nativeStatus.text = "${getString(R.string.native_version_label)}: ${roms.size} ROMs indexed"
                    }
                    .onFailure {
                        nativeStatus.text = "${getString(R.string.native_version_label)}: Scan failed"
                    }
            }
        }
        scanThread = thread
        thread.start()
    }

    private fun cancelLibraryScan() {
        scanGeneration += 1
        scanThread?.interrupt()
        scanThread = null
        scanButton.text = "Scan Folder"
        nativeStatus.text = "${getString(R.string.native_version_label)}: Scan canceled"
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

    private fun showAboutDialog() {
        val message = listOf(
            "Native core: ${NativeBridge.versionLabel()}",
            "mGBA copyright (c) 2013-2026 Jeffrey Pfau.",
            "mGBA is distributed under the Mozilla Public License 2.0.",
            "Third-party notices are preserved in the source tree and release artifacts.",
            "No commercial ROMs or BIOS files are bundled.",
        ).joinToString(separator = "\n\n")
        AlertDialog.Builder(this)
            .setTitle("About mGBA")
            .setMessage(message)
            .setPositiveButton("OK", null)
            .show()
    }

    private fun exportLogs() {
        nativeStatus.text = "${getString(R.string.native_version_label)}: Exporting logs"
        Thread {
            val uri = LogExporter.exportRecent(this)
            runOnUiThread {
                nativeStatus.text = "${getString(R.string.native_version_label)}: ${if (uri != null) "Logs exported" else "Log export unavailable"}"
            }
        }.start()
    }

    companion object {
        private const val REQUEST_OPEN_ROM = 1001
        private const val REQUEST_IMPORT_BIOS = 1002
        private const val REQUEST_IMPORT_PATCH = 1003
        private const val REQUEST_SCAN_FOLDER = 1004
        private const val MAX_LIBRARY_ITEMS = 24
    }
}

private enum class LibraryMode(val label: String) {
    All("All"),
    Favorites("Favorites"),
    Gba("GBA"),
    Gb("GB");

    fun next(): LibraryMode {
        val modes = entries
        return modes[(ordinal + 1) % modes.size]
    }

    fun matches(rom: LibraryRom): Boolean {
        return when (this) {
            All -> true
            Favorites -> rom.favorite
            Gba -> rom.platform.equals("GBA", ignoreCase = true)
            Gb -> rom.platform.equals("GB", ignoreCase = true)
        }
    }
}
