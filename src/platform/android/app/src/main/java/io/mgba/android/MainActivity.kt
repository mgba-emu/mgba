package io.mgba.android

import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.database.Cursor
import android.graphics.BitmapFactory
import android.net.Uri
import android.os.Bundle
import android.os.ParcelFileDescriptor
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
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import io.mgba.android.bridge.NativeBridge
import io.mgba.android.emulator.EmulatorController
import io.mgba.android.emulator.EmulatorSession
import io.mgba.android.library.LibraryRom
import io.mgba.android.library.RomLibraryStore
import io.mgba.android.library.RomScanner
import io.mgba.android.library.RecentGameStore
import io.mgba.android.settings.AudioBufferModes
import io.mgba.android.settings.AudioLowPassModes
import io.mgba.android.settings.EmulatorPreferences
import io.mgba.android.settings.PerGameOverrideStore
import io.mgba.android.storage.BiosStore
import io.mgba.android.storage.LogExporter
import io.mgba.android.storage.PatchStore
import java.io.File
import java.security.MessageDigest
import java.util.zip.ZipInputStream

class MainActivity : Activity() {
    private lateinit var nativeStatus: TextView
    private lateinit var recentStore: RecentGameStore
    private lateinit var libraryStore: RomLibraryStore
    private lateinit var preferences: EmulatorPreferences
    private lateinit var perGameOverrides: PerGameOverrideStore
    private lateinit var biosStore: BiosStore
    private lateinit var patchStore: PatchStore
    private lateinit var scanButton: Button
    private lateinit var biosButton: Button
    private lateinit var skipBiosButton: Button
    private lateinit var audioBufferButton: Button
    private lateinit var audioLowPassButton: Button
    private lateinit var patchButton: Button
    private lateinit var recentContainer: LinearLayout
    private lateinit var librarySearch: EditText
    private lateinit var libraryFilterButton: Button
    private lateinit var libraryContainer: LinearLayout
    private var libraryFilter = ""
    private var libraryMode = LibraryMode.All
    private var pendingCoverRomUri: Uri? = null
    private var scanThread: Thread? = null
    @Volatile
    private var scanGeneration = 0

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        recentStore = RecentGameStore(this)
        libraryStore = RomLibraryStore(this)
        preferences = EmulatorPreferences(this)
        perGameOverrides = PerGameOverrideStore(this)
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

        skipBiosButton = Button(this).apply {
            setOnClickListener {
                preferences.skipBios = !preferences.skipBios
                updateSkipBiosButton()
            }
        }
        updateSkipBiosButton()

        audioBufferButton = Button(this).apply {
            setOnClickListener {
                preferences.audioBufferMode = (preferences.audioBufferMode + 1) % AudioBufferModes.labels.size
                updateAudioBufferButton()
            }
        }
        updateAudioBufferButton()

        audioLowPassButton = Button(this).apply {
            setOnClickListener {
                preferences.audioLowPassMode = (preferences.audioLowPassMode + 1) % AudioLowPassModes.labels.size
                updateAudioLowPassButton()
            }
        }
        updateAudioLowPassButton()

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

        val clearArchiveCacheButton = Button(this).apply {
            text = "Clear ZIP Cache"
            setOnClickListener {
                clearArchiveCache()
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
        root.addView(skipBiosButton)
        root.addView(audioBufferButton)
        root.addView(audioLowPassButton)
        root.addView(patchButton)
        root.addView(aboutButton)
        root.addView(logButton)
        root.addView(clearArchiveCacheButton)
        root.addView(recentContainer)
        root.addView(librarySearch)
        root.addView(libraryFilterButton)
        root.addView(libraryContainer)
        scroll.addView(root)
        setContentView(scroll)
        renderRecentGames()
        renderLibrary()
    }

    override fun onTrimMemory(level: Int) {
        super.onTrimMemory(level)
        if (level >= TRIM_MEMORY_RUNNING_LOW_LEVEL) {
            trimArchiveCache(maxBytes = ARCHIVE_CACHE_TRIM_BYTES)
        }
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
            REQUEST_IMPORT_COVER -> importCover(uri)
        }
    }

    private fun openRomUri(uri: Uri, name: String, shouldStoreRecent: Boolean) {
        if (isZipArchive(name)) {
            openZipRomUri(uri, name, shouldStoreRecent)
            return
        }
        launchRomFd(uri, name, shouldStoreRecent) {
            contentResolver.openFileDescriptor(uri, "r")
        }
    }

    private fun openZipRomUri(uri: Uri, name: String, shouldStoreRecent: Boolean) {
        val entries = runCatching { zipRomEntries(uri) }.getOrDefault(emptyList())
        when (entries.size) {
            0 -> {
                nativeStatus.text = "${getString(R.string.native_version_label)}: No supported ROM found in ZIP"
            }
            1 -> launchZipRomEntry(uri, name, entries.first(), shouldStoreRecent)
            else -> {
                val labels = entries.map { it.substringAfterLast('/') }.toTypedArray()
                AlertDialog.Builder(this)
                    .setTitle("Select ROM")
                    .setItems(labels) { _, which ->
                        launchZipRomEntry(uri, name, entries[which], shouldStoreRecent)
                    }
                    .setNegativeButton("Cancel", null)
                    .show()
            }
        }
    }

    private fun launchZipRomEntry(uri: Uri, archiveName: String, entryName: String, shouldStoreRecent: Boolean) {
        val extracted = runCatching { extractZipRomEntry(uri, entryName) }.getOrNull()
        if (extracted == null) {
            nativeStatus.text = "${getString(R.string.native_version_label)}: ZIP extract failed"
            return
        }
        launchRomFd(
            uri,
            entryName.substringAfterLast('/').ifBlank { archiveName },
            shouldStoreRecent,
            recentDisplayName = archiveName,
        ) {
            ParcelFileDescriptor.open(extracted, ParcelFileDescriptor.MODE_READ_ONLY)
        }
    }

    private fun launchRomFd(
        uri: Uri,
        name: String,
        shouldStoreRecent: Boolean,
        recentDisplayName: String = name,
        openDescriptor: () -> ParcelFileDescriptor?,
    ) {
        val gameId = uri.toString()
        var patchApplied: Boolean? = null
        val result = runCatching {
            openDescriptor()?.use { descriptor ->
                val emulator = EmulatorSession.controller(this)
                emulator.setSkipBios(perGameOverrides.skipBios(gameId, preferences.skipBios))
                emulator.setAudioBufferSamples(
                    AudioBufferModes.samplesFor(
                        perGameOverrides.audioBufferMode(gameId, preferences.audioBufferMode),
                    ),
                )
                emulator.setLowPassRangePercent(
                    AudioLowPassModes.rangeFor(
                        perGameOverrides.audioLowPassMode(gameId, preferences.audioLowPassMode),
                    ),
                )
                emulator.loadRomFd(descriptor.fd, name).also { loadResult ->
                    if (loadResult.ok) {
                        patchApplied = applyStoredPatch(emulator, gameId)
                    }
                }
            }
        }.getOrNull()
        nativeStatus.text = if (result?.ok == true) {
            val patchStatus = when (patchApplied) {
                true -> " + patch"
                false -> " + patch failed"
                null -> ""
            }
            "${getString(R.string.native_version_label)}: ${result.platform} ${result.title}$patchStatus"
        } else {
            "${getString(R.string.native_version_label)}: ${result?.message ?: "Unable to open ROM"}"
        }
        if (result?.ok == true) {
            EmulatorSession.setCurrentGame(gameId, name)
            if (shouldStoreRecent) {
                recentStore.add(uri, recentDisplayName)
                renderRecentGames()
            }
            libraryStore.markPlayed(uri)
            renderLibrary()
            startActivity(Intent(this, EmulatorActivity::class.java))
        }
    }

    private fun applyStoredPatch(emulator: EmulatorController, gameId: String): Boolean? {
        val file = patchStore.fileForGame(gameId) ?: return null
        return runCatching {
            ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY).use { descriptor ->
                emulator.importPatchFd(descriptor.fd)
            }
        }.getOrDefault(false)
    }

    private fun zipRomEntries(uri: Uri): List<String> {
        val entries = mutableListOf<String>()
        contentResolver.openInputStream(uri)?.buffered()?.use { input ->
            ZipInputStream(input).use { zip ->
                while (true) {
                    val entry = zip.nextEntry ?: break
                    if (!entry.isDirectory && isSupportedRomEntry(entry.name)) {
                        entries += entry.name
                    }
                    zip.closeEntry()
                }
            }
        }
        return entries
    }

    private fun extractZipRomEntry(uri: Uri, entryName: String): File? {
        val target = archiveCacheFile(uri, entryName)
        val tmp = File(target.parentFile, "${target.name}.tmp")
        target.parentFile?.mkdirs()
        contentResolver.openInputStream(uri)?.buffered()?.use { input ->
            ZipInputStream(input).use { zip ->
                while (true) {
                    val entry = zip.nextEntry ?: break
                    if (!entry.isDirectory && entry.name == entryName) {
                        tmp.outputStream().use { output ->
                            zip.copyTo(output)
                        }
                        zip.closeEntry()
                        if (target.exists()) {
                            target.delete()
                        }
                        if (!tmp.renameTo(target)) {
                            return null
                        }
                        target.setLastModified(System.currentTimeMillis())
                        trimArchiveCache(keep = target)
                        return target
                    }
                    zip.closeEntry()
                }
            }
        }
        tmp.delete()
        return null
    }

    private fun archiveCacheFile(uri: Uri, entryName: String): File {
        val extension = entryName.substringAfterLast('.', "").takeIf { it.isNotBlank() }?.let { ".$it" } ?: ".rom"
        return File(File(cacheDir, "archive-roms"), "${sha1("${uri}\n$entryName")}$extension")
    }

    private fun isZipArchive(name: String): Boolean {
        return name.lowercase().endsWith(".zip")
    }

    private fun isSupportedRomEntry(name: String): Boolean {
        val lower = name.lowercase()
        return ROM_ENTRY_EXTENSIONS.any { lower.endsWith(it) }
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

    private fun updateSkipBiosButton() {
        skipBiosButton.text = if (preferences.skipBios) "Skip BIOS: On" else "Skip BIOS: Off"
    }

    private fun updateAudioBufferButton() {
        audioBufferButton.text = "Audio Buffer: ${AudioBufferModes.nameFor(preferences.audioBufferMode)}"
    }

    private fun updateAudioLowPassButton() {
        audioLowPassButton.text = "Low Pass: ${AudioLowPassModes.nameFor(preferences.audioLowPassMode)}"
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
                thumbnailView(rom)?.let { thumbnail ->
                    addView(thumbnail)
                }
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
                    text = "Cover"
                    setOnClickListener {
                        showCoverActions(rom)
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

    private fun showCoverActions(rom: LibraryRom) {
        val actions = if (rom.coverPath.isBlank()) {
            arrayOf("Import")
        } else {
            arrayOf("Import", "Clear")
        }
        AlertDialog.Builder(this)
            .setTitle("Cover")
            .setItems(actions) { _, which ->
                when (actions[which]) {
                    "Import" -> openCoverPicker(rom)
                    "Clear" -> clearCover(rom)
                }
            }
            .show()
    }

    private fun openCoverPicker(rom: LibraryRom) {
        pendingCoverRomUri = rom.uri
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "image/*"
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        startActivityForResult(intent, REQUEST_IMPORT_COVER)
    }

    private fun importCover(imageUri: Uri) {
        val romUri = pendingCoverRomUri ?: return
        pendingCoverRomUri = null
        nativeStatus.text = "${getString(R.string.native_version_label)}: Importing cover"
        Thread {
            val coverPath = copyCoverImage(romUri, imageUri)
            runOnUiThread {
                if (coverPath == null) {
                    nativeStatus.text = "${getString(R.string.native_version_label)}: Cover import failed"
                    Toast.makeText(this, "Cover import failed", Toast.LENGTH_SHORT).show()
                } else {
                    libraryStore.setCoverPath(romUri, coverPath)
                    renderLibrary()
                    nativeStatus.text = "${getString(R.string.native_version_label)}: Cover imported"
                    Toast.makeText(this, "Cover imported", Toast.LENGTH_SHORT).show()
                }
            }
        }.start()
    }

    private fun copyCoverImage(romUri: Uri, imageUri: Uri): String? {
        return runCatching {
            val coversDir = File(filesDir, "covers")
            coversDir.mkdirs()
            val target = File(coversDir, "${sha1(romUri.toString())}.cover")
            contentResolver.openInputStream(imageUri)?.use { input ->
                target.outputStream().use { output ->
                    input.copyTo(output)
                }
            } ?: return@runCatching null
            if (BitmapFactory.decodeFile(target.absolutePath) == null) {
                target.delete()
                null
            } else {
                target.absolutePath
            }
        }.getOrNull()
    }

    private fun clearCover(rom: LibraryRom) {
        val path = rom.coverPath
        if (path.isNotBlank()) {
            File(path).delete()
        }
        libraryStore.setCoverPath(rom.uri, "")
        renderLibrary()
        nativeStatus.text = "${getString(R.string.native_version_label)}: Cover cleared"
    }

    private fun thumbnailView(rom: LibraryRom): ImageView? {
        val path = rom.coverPath.takeIf { it.isNotBlank() } ?: return null
        if (!File(path).isFile) {
            return null
        }
        val bitmap = BitmapFactory.decodeFile(path) ?: return null
        return ImageView(this).apply {
            setImageBitmap(bitmap)
            scaleType = ImageView.ScaleType.CENTER_CROP
            layoutParams = LinearLayout.LayoutParams(dp(64), dp(48)).apply {
                rightMargin = dp(8)
            }
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
        val playTime = rom.playTimeSeconds.takeIf { it > 0L }?.let { "\nPlayed ${formatDuration(it)}" }.orEmpty()
        return "${libraryTitleLine(rom)}\nLast played $relative$playTime"
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

    private fun formatDuration(seconds: Long): String {
        val hours = seconds / 3600
        val minutes = (seconds % 3600) / 60
        return if (hours > 0) {
            "${hours}h ${minutes}m"
        } else {
            "${minutes.coerceAtLeast(1)}m"
        }
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
                        libraryStore.mergeScan(uri, roms)
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

    private fun sha1(value: String): String {
        val bytes = MessageDigest.getInstance("SHA-1").digest(value.toByteArray(Charsets.UTF_8))
        return bytes.joinToString("") { "%02x".format(it.toInt() and 0xff) }
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

    private fun clearArchiveCache() {
        val directory = File(cacheDir, "archive-roms")
        val deleted = directory.listFiles()?.count { it.delete() } ?: 0
        nativeStatus.text = "${getString(R.string.native_version_label)}: ZIP cache cleared ($deleted files)"
    }

    private fun trimArchiveCache(keep: File? = null, maxBytes: Long = ARCHIVE_CACHE_MAX_BYTES) {
        val directory = File(cacheDir, "archive-roms")
        val files = directory.listFiles()?.filter { it.isFile } ?: return
        val keepPath = keep?.absolutePath
        var totalBytes = files.sumOf { it.length() }
        files
            .filter { it.absolutePath != keepPath }
            .sortedBy { it.lastModified() }
            .forEach { file ->
                if (totalBytes <= maxBytes) {
                    return@forEach
                }
                val size = file.length()
                if (file.delete()) {
                    totalBytes -= size
                }
            }
    }

    companion object {
        private const val REQUEST_OPEN_ROM = 1001
        private const val REQUEST_IMPORT_BIOS = 1002
        private const val REQUEST_IMPORT_PATCH = 1003
        private const val REQUEST_SCAN_FOLDER = 1004
        private const val REQUEST_IMPORT_COVER = 1005
        private const val MAX_LIBRARY_ITEMS = 24
        private const val TRIM_MEMORY_RUNNING_LOW_LEVEL = 10
        private const val ARCHIVE_CACHE_MAX_BYTES = 256L * 1024L * 1024L
        private const val ARCHIVE_CACHE_TRIM_BYTES = 64L * 1024L * 1024L
        private val ROM_ENTRY_EXTENSIONS = arrayOf(".gba", ".agb", ".gb", ".gbc", ".sgb")
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
