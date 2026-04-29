package io.mgba.android

import android.app.Activity
import android.app.ActivityManager
import android.app.AlertDialog
import android.app.ApplicationExitInfo
import android.content.Intent
import android.graphics.Bitmap
import android.database.Cursor
import android.graphics.BitmapFactory
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.ParcelFileDescriptor
import android.os.SystemClock
import android.provider.OpenableColumns
import android.text.Editable
import android.text.InputType
import android.text.TextWatcher
import android.text.format.DateUtils
import android.util.LruCache
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
import io.mgba.android.bridge.NativeArchiveEntry
import io.mgba.android.bridge.NativeBridge
import io.mgba.android.bridge.NativeLoadResult
import io.mgba.android.emulator.EmulatorController
import io.mgba.android.emulator.EmulatorSession
import io.mgba.android.library.LibraryRom
import io.mgba.android.library.RomFileSupport
import io.mgba.android.library.RomIdentity
import io.mgba.android.library.RomLibraryStore
import io.mgba.android.library.RomScanner
import io.mgba.android.library.RecentGameStore
import io.mgba.android.settings.AudioBufferModes
import io.mgba.android.settings.AudioLowPassModes
import io.mgba.android.settings.EmulatorPreferences
import io.mgba.android.settings.FastForwardModes
import io.mgba.android.settings.InputMappingStore
import io.mgba.android.settings.LogLevelModes
import io.mgba.android.settings.PerGameOverrideStore
import io.mgba.android.settings.RewindSettings
import io.mgba.android.settings.RtcModes
import io.mgba.android.storage.AppLogStore
import io.mgba.android.storage.BiosStore
import io.mgba.android.storage.BiosSlot
import io.mgba.android.storage.CheatStore
import io.mgba.android.storage.LogExporter
import io.mgba.android.storage.PatchStore
import java.io.File
import java.security.MessageDigest
import org.json.JSONObject
import java.util.zip.ZipInputStream

class MainActivity : Activity() {
    private val coverThumbnailCache = object : LruCache<String, Bitmap>(COVER_THUMBNAIL_CACHE_BYTES) {
        override fun sizeOf(key: String, value: Bitmap): Int {
            return value.byteCount
        }
    }
    private lateinit var nativeStatus: TextView
    private lateinit var recentStore: RecentGameStore
    private lateinit var libraryStore: RomLibraryStore
    private lateinit var preferences: EmulatorPreferences
    private lateinit var perGameOverrides: PerGameOverrideStore
    private lateinit var inputMappingStore: InputMappingStore
    private lateinit var biosStore: BiosStore
    private lateinit var cheatStore: CheatStore
    private lateinit var patchStore: PatchStore
    private lateinit var scanButton: Button
    private lateinit var rescanFoldersButton: Button
    private lateinit var libraryFoldersButton: Button
    private lateinit var biosButton: Button
    private lateinit var clearBiosButton: Button
    private lateinit var skipBiosButton: Button
    private lateinit var scaleButton: Button
    private lateinit var filterButton: Button
    private lateinit var interframeBlendButton: Button
    private lateinit var audioBufferButton: Button
    private lateinit var audioLowPassButton: Button
    private lateinit var fastForwardModeButton: Button
    private lateinit var fastForwardSpeedButton: Button
    private lateinit var frameSkipButton: Button
    private lateinit var rewindButton: Button
    private lateinit var rewindBufferButton: Button
    private lateinit var rewindIntervalButton: Button
    private lateinit var autoStateButton: Button
    private lateinit var opposingDirectionsButton: Button
    private lateinit var rumbleButton: Button
    private lateinit var logLevelButton: Button
    private lateinit var rtcButton: Button
    private lateinit var patchButton: Button
    private lateinit var storageButton: Button
    private lateinit var recentContainer: LinearLayout
    private lateinit var librarySearch: EditText
    private lateinit var libraryFilterButton: Button
    private lateinit var libraryViewButton: Button
    private lateinit var libraryContainer: LinearLayout
    private var libraryFilter = ""
    private var libraryMode = LibraryMode.All
    private var libraryViewMode = LibraryViewMode.List
    private var pendingBiosSlot = BiosSlot.Default
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
        inputMappingStore = InputMappingStore(this)
        biosStore = BiosStore(this)
        cheatStore = CheatStore(this)
        patchStore = PatchStore(this)
        libraryViewMode = LibraryViewMode.fromName(
            getPreferences(MODE_PRIVATE).getString(KEY_LIBRARY_VIEW_MODE, null),
        )

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
        rescanFoldersButton = Button(this).apply {
            setOnClickListener {
                if (scanThread?.isAlive == true) {
                    cancelLibraryScan()
                } else {
                    rescanKnownFolders()
                }
            }
        }
        updateRescanFoldersButton()
        libraryFoldersButton = Button(this).apply {
            setOnClickListener {
                showLibraryFoldersDialog()
            }
        }
        updateLibraryFoldersButton()

        biosButton = Button(this).apply {
            setOnClickListener {
                showBiosImportDialog()
            }
        }
        clearBiosButton = Button(this).apply {
            setOnClickListener {
                showBiosClearDialog()
            }
        }
        updateBiosButtons()

        skipBiosButton = Button(this).apply {
            setOnClickListener {
                preferences.skipBios = !preferences.skipBios
                updateSkipBiosButton()
            }
        }
        updateSkipBiosButton()

        scaleButton = Button(this).apply {
            setOnClickListener {
                preferences.scaleMode = (preferences.scaleMode + 1) % SCALE_LABELS.size
                updateVideoButtons()
            }
        }
        filterButton = Button(this).apply {
            setOnClickListener {
                preferences.filterMode = (preferences.filterMode + 1) % FILTER_LABELS.size
                updateVideoButtons()
            }
        }
        interframeBlendButton = Button(this).apply {
            setOnClickListener {
                preferences.interframeBlending = !preferences.interframeBlending
                updateVideoButtons()
            }
        }
        updateVideoButtons()

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

        fastForwardModeButton = Button(this).apply {
            setOnClickListener {
                preferences.fastForwardMode = if (preferences.fastForwardMode == FastForwardModes.ModeToggle) {
                    FastForwardModes.ModeHold
                } else {
                    FastForwardModes.ModeToggle
                }
                updateFastForwardButtons()
            }
        }
        fastForwardSpeedButton = Button(this).apply {
            setOnClickListener {
                preferences.fastForwardMultiplier = FastForwardModes.nextMultiplier(preferences.fastForwardMultiplier)
                updateFastForwardButtons()
            }
        }
        updateFastForwardButtons()

        frameSkipButton = Button(this).apply {
            setOnClickListener {
                preferences.frameSkip = (preferences.frameSkip + 1) % FRAME_SKIP_LABELS.size
                updateFrameSkipButton()
            }
        }
        updateFrameSkipButton()

        rewindButton = Button(this).apply {
            setOnClickListener {
                preferences.rewindEnabled = !preferences.rewindEnabled
                updateRewindButtons()
            }
        }
        rewindBufferButton = Button(this).apply {
            setOnClickListener {
                preferences.rewindBufferCapacity = RewindSettings.nextCapacity(preferences.rewindBufferCapacity)
                updateRewindButtons()
            }
        }
        rewindIntervalButton = Button(this).apply {
            setOnClickListener {
                preferences.rewindBufferInterval = RewindSettings.nextInterval(preferences.rewindBufferInterval)
                updateRewindButtons()
            }
        }
        autoStateButton = Button(this).apply {
            setOnClickListener {
                preferences.autoStateOnExit = !preferences.autoStateOnExit
                updateRewindButtons()
            }
        }
        updateRewindButtons()

        opposingDirectionsButton = Button(this).apply {
            setOnClickListener {
                preferences.allowOpposingDirections = !preferences.allowOpposingDirections
                updateOpposingDirectionsButton()
            }
        }
        updateOpposingDirectionsButton()

        rumbleButton = Button(this).apply {
            setOnClickListener {
                preferences.rumbleEnabled = !preferences.rumbleEnabled
                updateRumbleButton()
            }
        }
        updateRumbleButton()

        logLevelButton = Button(this).apply {
            setOnClickListener {
                preferences.logLevelMode = LogLevelModes.next(preferences.logLevelMode)
                updateLogLevelButton()
            }
        }
        updateLogLevelButton()

        rtcButton = Button(this).apply {
            setOnClickListener {
                preferences.rtcMode = RtcModes.next(preferences.rtcMode)
                when (preferences.rtcMode) {
                    RtcModes.ModeFixed, RtcModes.ModeFakeEpoch -> preferences.rtcFixedTimeMs = System.currentTimeMillis()
                    RtcModes.ModeWallClockOffset -> preferences.rtcOffsetMs = 0L
                }
                updateRtcButton()
            }
        }
        updateRtcButton()

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

        storageButton = Button(this).apply {
            text = "Storage"
            setOnClickListener {
                showStorageDialog()
            }
        }

        val clearArchiveCacheButton = Button(this).apply {
            text = "Clear Cache"
            setOnClickListener {
                clearArchiveCache()
            }
        }
        val exportSettingsButton = Button(this).apply {
            text = "Export Settings"
            setOnClickListener {
                openSettingsExportPicker()
            }
        }
        val importSettingsButton = Button(this).apply {
            text = "Import Settings"
            setOnClickListener {
                openSettingsImportPicker()
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
        libraryViewButton = Button(this).apply {
            visibility = View.GONE
            setOnClickListener {
                libraryViewMode = libraryViewMode.next()
                getPreferences(MODE_PRIVATE)
                    .edit()
                    .putString(KEY_LIBRARY_VIEW_MODE, libraryViewMode.name)
                    .apply()
                renderLibrary()
            }
        }

        root.addView(title)
        root.addView(subtitle)
        root.addView(nativeStatus)
        root.addView(openButton)
        root.addView(scanButton)
        root.addView(rescanFoldersButton)
        root.addView(libraryFoldersButton)
        root.addView(biosButton)
        root.addView(clearBiosButton)
        root.addView(skipBiosButton)
        root.addView(scaleButton)
        root.addView(filterButton)
        root.addView(interframeBlendButton)
        root.addView(audioBufferButton)
        root.addView(audioLowPassButton)
        root.addView(fastForwardModeButton)
        root.addView(fastForwardSpeedButton)
        root.addView(frameSkipButton)
        root.addView(rewindButton)
        root.addView(rewindBufferButton)
        root.addView(rewindIntervalButton)
        root.addView(autoStateButton)
        root.addView(opposingDirectionsButton)
        root.addView(rumbleButton)
        root.addView(logLevelButton)
        root.addView(rtcButton)
        root.addView(patchButton)
        root.addView(aboutButton)
        root.addView(logButton)
        root.addView(storageButton)
        root.addView(clearArchiveCacheButton)
        root.addView(exportSettingsButton)
        root.addView(importSettingsButton)
        root.addView(recentContainer)
        root.addView(librarySearch)
        root.addView(libraryFilterButton)
        root.addView(libraryViewButton)
        root.addView(libraryContainer)
        scroll.addView(root)
        setContentView(scroll)
        showCrashRecoveryPromptIfNeeded()
        renderRecentGames()
        renderLibrary()
        handleViewIntent(intent)
    }

    override fun onNewIntent(intent: Intent?) {
        super.onNewIntent(intent)
        setIntent(intent)
        handleViewIntent(intent)
    }

    override fun onTrimMemory(level: Int) {
        super.onTrimMemory(level)
        if (level >= TRIM_MEMORY_RUNNING_LOW_LEVEL) {
            trimArchiveCache(maxBytes = ARCHIVE_CACHE_TRIM_BYTES)
            trimArchiveFileCache(maxBytes = ARCHIVE_CACHE_TRIM_BYTES)
            trimImportCache(maxBytes = ARCHIVE_CACHE_TRIM_BYTES)
            coverThumbnailCache.evictAll()
        } else if (level >= TRIM_MEMORY_UI_HIDDEN) {
            coverThumbnailCache.trimToSize(COVER_THUMBNAIL_CACHE_BYTES / 2)
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
                val ok = biosStore.import(pendingBiosSlot, uri, name)
                updateBiosButtons()
                nativeStatus.text = "${getString(R.string.native_version_label)}: ${if (ok) "${pendingBiosSlot.label} BIOS imported" else "BIOS import failed"}"
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
            REQUEST_EXPORT_SETTINGS -> exportSettings(uri)
            REQUEST_IMPORT_SETTINGS -> importSettings(uri)
            REQUEST_EXPORT_LOGS -> exportLogsToUri(uri)
        }
    }

    private fun openRomUri(uri: Uri, name: String, shouldStoreRecent: Boolean) {
        if (isZipArchive(name)) {
            openZipRomUri(uri, name, shouldStoreRecent)
            return
        }
        if (isSevenZipArchive(name)) {
            openNativeArchiveRomUri(uri, name, shouldStoreRecent)
            return
        }
        launchRomFd(uri, name, shouldStoreRecent, romSha1 = { sha1(uri) }) {
            contentResolver.openFileDescriptor(uri, "r")
        }
    }

    private fun handleViewIntent(intent: Intent?) {
        if (intent?.action != Intent.ACTION_VIEW) {
            return
        }
        val uri = intent.data ?: return
        runCatching {
            val flags = intent.flags and Intent.FLAG_GRANT_READ_URI_PERMISSION
            if (flags != 0) {
                contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
            }
        }
        val name = displayName(uri)
        if (!RomFileSupport.isSupportedRomName(name)) {
            nativeStatus.text = "${getString(R.string.native_version_label)}: Unsupported ROM file"
            return
        }
        openRomUri(uri, name, shouldStoreRecent = true)
    }

    private fun openZipRomUri(uri: Uri, name: String, shouldStoreRecent: Boolean) {
        val entries = runCatching { zipRomEntries(uri) }.getOrDefault(emptyList())
        when (entries.size) {
            0 -> {
                openNativeArchiveRomUri(uri, name, shouldStoreRecent)
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
            allowImportFallback = false,
            preferStoredSha1 = false,
            romSha1 = { sha1(extracted) },
        ) {
            ParcelFileDescriptor.open(extracted, ParcelFileDescriptor.MODE_READ_ONLY)
        }
    }

    private fun openNativeArchiveRomUri(uri: Uri, name: String, shouldStoreRecent: Boolean) {
        nativeStatus.text = "${getString(R.string.native_version_label)}: Reading archive"
        Thread {
            val archive = runCatching { cacheArchiveFile(uri, name) }.getOrNull()
            val entries = archive?.let { NativeBridge.archiveRomEntries(it.absolutePath) }.orEmpty()
            runOnUiThread {
                when {
                    archive == null -> {
                        nativeStatus.text = "${getString(R.string.native_version_label)}: Archive cache failed"
                    }
                    entries.isEmpty() -> {
                        nativeStatus.text = "${getString(R.string.native_version_label)}: No supported ROM found in archive"
                    }
                    entries.size == 1 -> {
                        launchNativeArchiveRomEntry(uri, name, archive, entries.first(), shouldStoreRecent)
                    }
                    else -> {
                        val labels = entries.map { it.displayName }.toTypedArray()
                        AlertDialog.Builder(this)
                            .setTitle("Select ROM")
                            .setItems(labels) { _, which ->
                                launchNativeArchiveRomEntry(uri, name, archive, entries[which], shouldStoreRecent)
                            }
                            .setNegativeButton("Cancel", null)
                            .show()
                    }
                }
            }
        }.start()
    }

    private fun launchNativeArchiveRomEntry(
        uri: Uri,
        archiveName: String,
        archive: File,
        entry: NativeArchiveEntry,
        shouldStoreRecent: Boolean,
    ) {
        nativeStatus.text = "${getString(R.string.native_version_label)}: Extracting archive"
        Thread {
            val extracted = runCatching { extractNativeArchiveRomEntry(archive, uri, entry.name) }.getOrNull()
            runOnUiThread {
                if (extracted == null) {
                    nativeStatus.text = "${getString(R.string.native_version_label)}: Archive extract failed"
                    return@runOnUiThread
                }
                launchRomFd(
                    uri,
                    entry.displayName.ifBlank { archiveName },
                    shouldStoreRecent,
                    recentDisplayName = archiveName,
                    allowImportFallback = false,
                    preferStoredSha1 = false,
                    romSha1 = { sha1(extracted) },
                ) {
                    ParcelFileDescriptor.open(extracted, ParcelFileDescriptor.MODE_READ_ONLY)
                }
            }
        }.start()
    }

    private fun launchRomFd(
        uri: Uri,
        name: String,
        shouldStoreRecent: Boolean,
        recentDisplayName: String = name,
        allowImportFallback: Boolean = true,
        preferStoredSha1: Boolean = true,
        romSha1: (() -> String)? = null,
        openDescriptor: () -> ParcelFileDescriptor?,
    ) {
        val launchStartedAtMs = SystemClock.elapsedRealtime()
        val gameId = uri.toString()
        val knownHashes = knownRomHashesFor(uri, preferStoredSha1)
        var computedSha1 = knownHashes.sha1.ifBlank { romSha1?.invoke().orEmpty() }
        var launchGameId = stableGameIdFor(gameId, knownHashes.crc32, computedSha1)
        val launchCrcGameId = crc32GameIdFor(gameId, knownHashes.crc32)
        perGameOverrides.migrateGameId(launchGameId, gameId)
        perGameOverrides.migrateGameId(launchGameId, launchCrcGameId)
        biosStore.migrateGameId(launchGameId, gameId)
        biosStore.migrateGameId(launchGameId, launchCrcGameId)
        var patchApplied: Boolean? = null
        var cheatsApplied: Boolean? = null
        var autoStateLoaded = false
        var usedImportFallback = false
        fun loadDescriptor(descriptor: ParcelFileDescriptor): NativeLoadResult {
            val emulator = EmulatorSession.controller(this)
            emulator.setBiosOverridePaths(
                biosStore.pathForGame(launchGameId, BiosSlot.Default),
                biosStore.pathForGame(launchGameId, BiosSlot.Gba),
                biosStore.pathForGame(launchGameId, BiosSlot.Gb),
                biosStore.pathForGame(launchGameId, BiosSlot.Gbc),
            )
            emulator.setSkipBios(perGameOverrides.skipBios(launchGameId, preferences.skipBios))
            emulator.setAudioBufferSamples(
                AudioBufferModes.samplesFor(
                    perGameOverrides.audioBufferMode(launchGameId, preferences.audioBufferMode),
                ),
            )
            emulator.setLowPassRangePercent(
                AudioLowPassModes.rangeFor(
                    perGameOverrides.audioLowPassMode(launchGameId, preferences.audioLowPassMode),
                ),
            )
            emulator.setFrameSkip(perGameOverrides.frameSkip(launchGameId, preferences.frameSkip))
            emulator.setInterframeBlending(
                perGameOverrides.interframeBlending(launchGameId, preferences.interframeBlending),
            )
            emulator.setLogLevelMode(preferences.logLevelMode)
            emulator.setRtcMode(preferences.rtcMode, rtcValueForMode(preferences.rtcMode))
            emulator.setRewindConfig(
                perGameOverrides.rewindEnabled(launchGameId, preferences.rewindEnabled),
                perGameOverrides.rewindBufferCapacity(launchGameId, preferences.rewindBufferCapacity),
                perGameOverrides.rewindBufferInterval(launchGameId, preferences.rewindBufferInterval),
            )
            return emulator.loadRomFd(descriptor.fd, name).also { loadResult ->
                if (loadResult.ok) {
                    val stableGameId = stableGameIdFor(gameId, loadResult.crc32, computedSha1)
                    val crcGameId = crc32GameIdFor(gameId, loadResult.crc32)
                    perGameOverrides.migrateGameId(stableGameId, gameId)
                    perGameOverrides.migrateGameId(stableGameId, crcGameId)
                    biosStore.migrateGameId(stableGameId, gameId)
                    biosStore.migrateGameId(stableGameId, crcGameId)
                    patchApplied = applyStoredPatch(emulator, gameId, stableGameId, crcGameId, name, loadResult.crc32)
                    cheatsApplied = applyStoredCheats(emulator, gameId, stableGameId, crcGameId)
                    autoStateLoaded = preferences.autoStateOnExit && emulator.loadAutoState()
                }
            }
        }

        var result = runCatching {
            openDescriptor()?.use { descriptor ->
                loadDescriptor(descriptor)
            }
        }.getOrNull()
        if (allowImportFallback && result?.ok != true) {
            val cached = runCatching { cacheImportFile(uri, name) }.getOrNull()
            if (cached != null) {
                if (computedSha1.isBlank()) {
                    computedSha1 = sha1(cached)
                    launchGameId = stableGameIdFor(gameId, knownHashes.crc32, computedSha1)
                    perGameOverrides.migrateGameId(launchGameId, gameId)
                    perGameOverrides.migrateGameId(launchGameId, launchCrcGameId)
                    biosStore.migrateGameId(launchGameId, gameId)
                    biosStore.migrateGameId(launchGameId, launchCrcGameId)
                }
                usedImportFallback = true
                patchApplied = null
                cheatsApplied = null
                result = runCatching {
                    ParcelFileDescriptor.open(cached, ParcelFileDescriptor.MODE_READ_ONLY).use { descriptor ->
                        loadDescriptor(descriptor)
                    }
                }.getOrNull()
            }
        }
        nativeStatus.text = if (result?.ok == true) {
            val patchStatus = when (patchApplied) {
                true -> " + patch"
                false -> " + patch failed"
                null -> ""
            }
            val cheatStatus = when (cheatsApplied) {
                true -> " + cheats"
                false -> " + cheats failed"
                null -> ""
            }
            val autoStateStatus = if (autoStateLoaded) " + auto-state" else ""
            val hardware = if (result.system.equals("CGB", ignoreCase = true)) "GBC" else result.platform
            val fallbackStatus = if (usedImportFallback) " + cache" else ""
            "${getString(R.string.native_version_label)}: $hardware ${result.title}$patchStatus$cheatStatus$autoStateStatus$fallbackStatus"
        } else {
            "${getString(R.string.native_version_label)}: ${result?.message ?: "Unable to open ROM"}"
        }
        AppLogStore.append(
            this,
            if (result?.ok == true) {
                val loadedAtMs = SystemClock.elapsedRealtime()
                "Loaded ROM $name (${result.platform}/${result.system.ifBlank { "unknown" }}, cacheFallback=$usedImportFallback, patch=${artifactStatus(patchApplied)}, cheats=${artifactStatus(cheatsApplied)}, autoState=$autoStateLoaded, loadMs=${loadedAtMs - launchStartedAtMs})"
            } else {
                "Failed to load ROM $name: ${result?.message ?: "Unable to open ROM"}"
            },
        )
        if (result?.ok == true) {
            val stableGameId = stableGameIdFor(gameId, result.crc32, computedSha1)
            EmulatorSession.setCurrentGame(
                gameId,
                name,
                stableGameId,
                result.crc32,
                computedSha1,
                launchStartedAtMs,
                SystemClock.elapsedRealtime(),
            )
            if (shouldStoreRecent) {
                recentStore.add(uri, recentDisplayName, stableGameId, result.crc32, computedSha1)
                renderRecentGames()
            }
            libraryStore.markPlayed(uri)
            renderLibrary()
            startActivity(Intent(this, EmulatorActivity::class.java))
        }
    }

    private fun applyStoredPatch(
        emulator: EmulatorController,
        gameId: String,
        stableGameId: String,
        crcGameId: String,
        displayName: String,
        crc32: String,
    ): Boolean? {
        val file = artifactGameIds(gameId, stableGameId, crcGameId)
            .asSequence()
            .mapNotNull { patchStore.fileForGame(it) }
            .firstOrNull()
            ?: patchStore.autoPatchFile(displayName, crc32)
            ?: return null
        return runCatching {
            ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY).use { descriptor ->
                emulator.importPatchFd(descriptor.fd)
            }
        }.getOrDefault(false)
    }

    private fun applyStoredCheats(
        emulator: EmulatorController,
        gameId: String,
        stableGameId: String,
        crcGameId: String,
    ): Boolean? {
        val file = artifactGameIds(gameId, stableGameId, crcGameId)
            .asSequence()
            .mapNotNull { cheatStore.fileForGame(it) }
            .firstOrNull()
            ?: return null
        return runCatching {
            ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY).use { descriptor ->
                emulator.importCheatsFd(descriptor.fd)
            }
        }.getOrDefault(false)
    }

    private fun stableGameIdFor(gameId: String, crc32: String, sha1: String): String {
        return RomIdentity.stableGameId(gameId, crc32, sha1)
    }

    private fun crc32GameIdFor(gameId: String, crc32: String): String {
        return RomIdentity.crc32GameId(gameId, crc32)
    }

    private fun knownRomHashesFor(uri: Uri, preferStoredSha1: Boolean): RomHashes {
        val libraryEntry = libraryStore.list().firstOrNull { it.uri == uri }
        val recentEntry = recentStore.list().firstOrNull { it.uri == uri }
        val recentStableId = recentEntry?.stableId.orEmpty()
        val recentCrc32 = recentEntry?.crc32?.ifBlank {
            recentStableId.substringAfter("crc32:", missingDelimiterValue = "").takeIf { it != recentStableId }.orEmpty()
        }.orEmpty()
        val recentSha1 = recentEntry?.sha1?.ifBlank {
            recentStableId.substringAfter("sha1:", missingDelimiterValue = "").takeIf { it != recentStableId }.orEmpty()
        }.orEmpty()
        return RomHashes(
            crc32 = libraryEntry?.crc32?.ifBlank { recentCrc32 } ?: recentCrc32,
            sha1 = if (preferStoredSha1) {
                libraryEntry?.sha1?.ifBlank { recentSha1 } ?: recentSha1
            } else {
                ""
            },
        )
    }

    private fun artifactGameIds(vararg gameIds: String?): List<String> {
        return gameIds.toList().filterNot { it.isNullOrBlank() }.map { it.orEmpty() }.distinct()
    }

    private fun artifactStatus(applied: Boolean?): String {
        return when (applied) {
            true -> "applied"
            false -> "failed"
            null -> "none"
        }
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

    private fun cacheArchiveFile(uri: Uri, name: String): File? {
        val extension = name.substringAfterLast('.', "").takeIf { it.isNotBlank() }?.let { ".$it" } ?: ".archive"
        val directory = File(cacheDir, "archive-files")
        val target = File(directory, "${sha1(uri.toString())}$extension")
        val tmp = File(directory, "${target.name}.tmp")
        directory.mkdirs()
        contentResolver.openInputStream(uri)?.use { input ->
            tmp.outputStream().use { output ->
                input.copyTo(output)
            }
        } ?: return null
        if (target.exists()) {
            target.delete()
        }
        if (!tmp.renameTo(target)) {
            tmp.delete()
            return null
        }
        target.setLastModified(System.currentTimeMillis())
        trimArchiveFileCache(keep = target)
        return target
    }

    private fun extractNativeArchiveRomEntry(archive: File, uri: Uri, entryName: String): File? {
        val target = archiveCacheFile(uri, entryName)
        val tmp = File(target.parentFile, "${target.name}.tmp")
        target.parentFile?.mkdirs()
        if (!NativeBridge.extractArchiveRomEntry(archive.absolutePath, entryName, tmp.absolutePath)) {
            tmp.delete()
            return null
        }
        if (target.exists()) {
            target.delete()
        }
        if (!tmp.renameTo(target)) {
            tmp.delete()
            return null
        }
        target.setLastModified(System.currentTimeMillis())
        trimArchiveCache(keep = target)
        return target
    }

    private fun archiveCacheFile(uri: Uri, entryName: String): File {
        val extension = entryName.substringAfterLast('.', "").takeIf { it.isNotBlank() }?.let { ".$it" } ?: ".rom"
        return File(File(cacheDir, "archive-roms"), "${sha1("${uri}\n$entryName")}$extension")
    }

    private fun cacheImportFile(uri: Uri, name: String): File? {
        val extension = name.substringAfterLast('.', "").takeIf { it.isNotBlank() }?.let { ".$it" } ?: ".rom"
        val directory = File(cacheDir, "imports")
        val target = File(directory, "${sha1(uri.toString())}$extension")
        val tmp = File(directory, "${target.name}.tmp")
        directory.mkdirs()
        contentResolver.openInputStream(uri)?.use { input ->
            tmp.outputStream().use { output ->
                input.copyTo(output)
            }
        } ?: return null
        if (target.exists()) {
            target.delete()
        }
        if (!tmp.renameTo(target)) {
            tmp.delete()
            return null
        }
        target.setLastModified(System.currentTimeMillis())
        trimImportCache(keep = target)
        return target
    }

    private fun isZipArchive(name: String): Boolean {
        return name.lowercase().endsWith(".zip")
    }

    private fun isSevenZipArchive(name: String): Boolean {
        return name.lowercase().endsWith(".7z")
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
        recentContainer.addView(Button(this).apply {
            text = "Clear Recent"
            setOnClickListener {
                val cleared = recentStore.clear()
                renderRecentGames()
                nativeStatus.text = "${getString(R.string.native_version_label)}: Recent cleared ($cleared)"
            }
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

    private fun updateBiosButtons() {
        val infos = biosStore.infos
        if (infos.isEmpty()) {
            biosButton.text = "Import BIOS"
            clearBiosButton.text = "Clear BIOS"
            clearBiosButton.isEnabled = false
        } else {
            val summary = infos.joinToString("  ") { info ->
                "${info.slot.label}:${info.sha1.take(8)}"
            }
            val totalBytes = infos.sumOf { it.sizeBytes }
            biosButton.text = "BIOS: $summary (${formatBytes(totalBytes)})"
            clearBiosButton.text = "Clear BIOS"
            clearBiosButton.isEnabled = true
        }
    }

    private fun showBiosImportDialog() {
        val slots = BiosSlot.entries.toTypedArray()
        val labels = slots.map { slot ->
            val info = biosStore.info(slot)
            if (info == null) {
                "${slot.label} BIOS"
            } else {
                "${slot.label}: ${info.displayName} (${formatBytes(info.sizeBytes)}, SHA1 ${info.sha1.take(8)})"
            }
        }.toTypedArray()
        AlertDialog.Builder(this)
            .setTitle("Import BIOS")
            .setItems(labels) { _, which ->
                pendingBiosSlot = slots[which]
                openBiosPicker()
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun showBiosClearDialog() {
        val infos = biosStore.infos
        if (infos.isEmpty()) {
            return
        }
        val labels = infos.map { info ->
            "${info.slot.label}: ${info.displayName}"
        }.toTypedArray()
        AlertDialog.Builder(this)
            .setTitle("Clear BIOS")
            .setItems(labels) { _, which ->
                val slot = infos[which].slot
                val ok = biosStore.clear(slot)
                updateBiosButtons()
                nativeStatus.text = "${getString(R.string.native_version_label)}: ${if (ok) "${slot.label} BIOS cleared" else "BIOS clear failed"}"
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun updateVideoButtons() {
        scaleButton.text = "Scale: ${SCALE_LABELS[preferences.scaleMode]}"
        filterButton.text = "Filter: ${FILTER_LABELS[preferences.filterMode]}"
        interframeBlendButton.text = if (preferences.interframeBlending) {
            "Interframe: On"
        } else {
            "Interframe: Off"
        }
    }

    private fun updateAudioBufferButton() {
        audioBufferButton.text = "Audio Buffer: ${AudioBufferModes.nameFor(preferences.audioBufferMode)}"
    }

    private fun updateAudioLowPassButton() {
        audioLowPassButton.text = "Low Pass: ${AudioLowPassModes.nameFor(preferences.audioLowPassMode)}"
    }

    private fun updateFastForwardButtons() {
        fastForwardModeButton.text = "Fast Mode: ${FastForwardModes.modeLabels[preferences.fastForwardMode]}"
        fastForwardSpeedButton.text = "Fast Speed: ${FastForwardModes.labelForMultiplier(preferences.fastForwardMultiplier)}"
    }

    private fun updateFrameSkipButton() {
        frameSkipButton.text = "Frame Skip: ${FRAME_SKIP_LABELS[preferences.frameSkip]}"
    }

    private fun updateRewindButtons() {
        rewindButton.text = if (preferences.rewindEnabled) "Rewind: On" else "Rewind: Off"
        rewindBufferButton.text = "Rewind Buffer: ${preferences.rewindBufferCapacity}"
        rewindIntervalButton.text = "Rewind Speed: ${preferences.rewindBufferInterval}"
        autoStateButton.text = if (preferences.autoStateOnExit) "Auto State: On" else "Auto State: Off"
    }

    private fun updateOpposingDirectionsButton() {
        opposingDirectionsButton.text = if (preferences.allowOpposingDirections) {
            "Opposite Directions: On"
        } else {
            "Opposite Directions: Off"
        }
    }

    private fun updateRumbleButton() {
        rumbleButton.text = if (preferences.rumbleEnabled) {
            "Rumble: On"
        } else {
            "Rumble: Off"
        }
    }

    private fun updateLogLevelButton() {
        logLevelButton.text = "Log Level: ${LogLevelModes.labels[preferences.logLevelMode]}"
    }

    private fun updatePreferenceButtons() {
        updateSkipBiosButton()
        updateVideoButtons()
        updateAudioBufferButton()
        updateAudioLowPassButton()
        updateFastForwardButtons()
        updateFrameSkipButton()
        updateRewindButtons()
        updateOpposingDirectionsButton()
        updateRumbleButton()
        updateLogLevelButton()
        updateRtcButton()
    }

    private fun rtcValueForMode(mode: Int): Long {
        return when (RtcModes.coerce(mode)) {
            RtcModes.ModeFixed, RtcModes.ModeFakeEpoch -> preferences.rtcFixedTimeMs
            RtcModes.ModeWallClockOffset -> preferences.rtcOffsetMs
            else -> 0L
        }
    }

    private fun updateRtcButton() {
        val mode = preferences.rtcMode
        rtcButton.text = when (mode) {
            RtcModes.ModeFixed, RtcModes.ModeFakeEpoch -> {
                val label = DateUtils.formatDateTime(
                    this,
                    preferences.rtcFixedTimeMs,
                    DateUtils.FORMAT_SHOW_DATE or DateUtils.FORMAT_SHOW_TIME,
                )
                "RTC: ${RtcModes.labels[mode]} $label"
            }
            RtcModes.ModeWallClockOffset -> "RTC: Offset ${preferences.rtcOffsetMs / DateUtils.MINUTE_IN_MILLIS}m"
            else -> "RTC: ${RtcModes.labels[mode]}"
        }
    }

    private fun renderLibrary() {
        libraryContainer.removeAllViews()
        val allRoms = libraryStore.list()
        librarySearch.visibility = if (allRoms.isEmpty()) View.GONE else View.VISIBLE
        libraryFilterButton.visibility = if (allRoms.isEmpty()) View.GONE else View.VISIBLE
        libraryViewButton.visibility = if (allRoms.isEmpty()) View.GONE else View.VISIBLE
        libraryFilterButton.text = "Filter: ${libraryMode.label}"
        libraryViewButton.text = "View: ${libraryViewMode.label}"
        if (allRoms.isEmpty()) {
            libraryContainer.addView(TextView(this).apply {
                text = "No ROMs yet"
                textSize = 14f
                setTextColor(getColor(R.color.mgba_text_secondary))
                setPadding(0, 0, 0, dp(8))
            })
            libraryContainer.addView(Button(this).apply {
                text = "Open ROM"
                setOnClickListener {
                    openRomPicker()
                }
            })
            libraryContainer.addView(Button(this).apply {
                text = "Add Folder"
                setOnClickListener {
                    openFolderPicker()
                }
            })
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
                    it.system.contains(query, ignoreCase = true) ||
                    it.hardwareLabel().contains(query, ignoreCase = true) ||
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
        val visibleRoms = roms.take(MAX_LIBRARY_ITEMS)
        if (libraryViewMode == LibraryViewMode.Grid) {
            renderLibraryGrid(visibleRoms)
        } else {
            visibleRoms.forEach { rom ->
                libraryContainer.addView(libraryListRow(rom))
            }
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

    private fun libraryListRow(rom: LibraryRom): LinearLayout {
        return LinearLayout(this).apply {
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
            addLibraryActionButtons(this, rom)
        }
    }

    private fun renderLibraryGrid(roms: List<LibraryRom>) {
        roms.chunked(LIBRARY_GRID_COLUMNS).forEach { chunk ->
            libraryContainer.addView(LinearLayout(this).apply {
                orientation = LinearLayout.HORIZONTAL
                chunk.forEach { rom ->
                    addView(libraryGridCell(rom))
                }
                repeat(LIBRARY_GRID_COLUMNS - chunk.size) {
                    addView(LinearLayout(context).apply {
                        visibility = View.INVISIBLE
                        layoutParams = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f)
                    })
                }
            })
        }
    }

    private fun libraryGridCell(rom: LibraryRom): LinearLayout {
        return LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(0, 0, dp(8), dp(10))
            layoutParams = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f)
            thumbnailView(rom, widthDp = 96, heightDp = 72, rightMarginDp = 0, bottomMarginDp = 4)?.let { thumbnail ->
                addView(thumbnail)
            }
            addView(Button(context).apply {
                text = libraryButtonLabel(rom)
                maxLines = 4
                setOnClickListener {
                    openRomUri(rom.uri, rom.displayName, shouldStoreRecent = true)
                }
                layoutParams = LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT,
                )
            })
            addView(LinearLayout(context).apply {
                orientation = LinearLayout.HORIZONTAL
                addLibraryActionButtons(this, rom)
            })
        }
    }

    private fun addLibraryActionButtons(container: LinearLayout, rom: LibraryRom) {
        container.addView(Button(container.context).apply {
            text = if (rom.favorite) "Fav*" else "Fav"
            setOnClickListener {
                libraryStore.toggleFavorite(rom.uri)
                renderLibrary()
            }
        })
        container.addView(Button(container.context).apply {
            text = "Cover"
            setOnClickListener {
                showCoverActions(rom)
            }
        })
        container.addView(Button(container.context).apply {
            text = "Del"
            setOnClickListener {
                confirmRemoveLibraryRom(rom)
            }
        })
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
                    coverThumbnailCache.evictAll()
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
        deleteCoverPath(rom.coverPath)
        libraryStore.setCoverPath(rom.uri, "")
        coverThumbnailCache.evictAll()
        renderLibrary()
        nativeStatus.text = "${getString(R.string.native_version_label)}: Cover cleared"
    }

    private fun thumbnailView(
        rom: LibraryRom,
        widthDp: Int = 64,
        heightDp: Int = 48,
        rightMarginDp: Int = 8,
        bottomMarginDp: Int = 0,
    ): ImageView? {
        val path = rom.coverPath.takeIf { it.isNotBlank() } ?: return null
        if (!File(path).isFile) {
            return null
        }
        val widthPx = dp(widthDp)
        val heightPx = dp(heightDp)
        val bitmap = coverThumbnailBitmap(path, widthPx, heightPx) ?: return null
        return ImageView(this).apply {
            setImageBitmap(bitmap)
            scaleType = ImageView.ScaleType.CENTER_CROP
            layoutParams = LinearLayout.LayoutParams(widthPx, heightPx).apply {
                rightMargin = dp(rightMarginDp)
                bottomMargin = dp(bottomMarginDp)
            }
        }
    }

    private fun coverThumbnailBitmap(path: String, widthPx: Int, heightPx: Int): Bitmap? {
        val file = File(path)
        val key = "${file.absolutePath}:${file.lastModified()}:$widthPx:$heightPx"
        coverThumbnailCache.get(key)?.let { return it }
        val bitmap = decodeCoverThumbnail(path, widthPx, heightPx) ?: return null
        coverThumbnailCache.put(key, bitmap)
        return bitmap
    }

    private fun decodeCoverThumbnail(path: String, widthPx: Int, heightPx: Int): Bitmap? {
        val bounds = BitmapFactory.Options().apply {
            inJustDecodeBounds = true
        }
        BitmapFactory.decodeFile(path, bounds)
        if (bounds.outWidth <= 0 || bounds.outHeight <= 0) {
            return null
        }
        var sample = 1
        val targetWidth = widthPx.coerceAtLeast(1)
        val targetHeight = heightPx.coerceAtLeast(1)
        while (bounds.outWidth / sample > targetWidth * 2 || bounds.outHeight / sample > targetHeight * 2) {
            sample *= 2
        }
        return BitmapFactory.decodeFile(
            path,
            BitmapFactory.Options().apply {
                inSampleSize = sample
            },
        )
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
        val platform = rom.hardwareLabel().takeIf { it.isNotBlank() }?.let { " [$it]" }.orEmpty()
        val code = rom.gameCode.takeIf { it.isNotBlank() }?.let { " $it" }.orEmpty()
        val maker = rom.maker.takeIf { it.isNotBlank() }?.let { "/$it" }.orEmpty()
        val version = rom.version.takeIf { it >= 0 }?.let { " v$it" }.orEmpty()
        val size = rom.fileSize.takeIf { it > 0L }?.let { " ${formatBytes(it)}" }.orEmpty()
        return "$marker$title$platform$code$maker$version$size"
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
                libraryStore.remove(rom.uri)?.let { removed ->
                    deleteCoverPath(removed.coverPath)
                    coverThumbnailCache.evictAll()
                }
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
            val result = runCatching {
                RomScanner(this) { count, name ->
                    runOnUiThread {
                        if (generation == scanGeneration) {
                            nativeStatus.text = "${getString(R.string.native_version_label)}: Scanning $count - $name"
                        }
                    }
                }.scan(uri)
            }
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
                        updateRescanFoldersButton()
                        updateLibraryFoldersButton()
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

    private fun rescanKnownFolders() {
        val sources = libraryStore.sourceFolders()
        if (sources.isEmpty()) {
            nativeStatus.text = "${getString(R.string.native_version_label)}: No folders added"
            return
        }
        val generation = ++scanGeneration
        scanThread?.interrupt()
        scanButton.text = "Cancel Scan"
        rescanFoldersButton.text = "Cancel Rescan"
        nativeStatus.text = "${getString(R.string.native_version_label)}: Rescanning folders"
        val thread = Thread {
            var total = 0
            var failed = 0
            sources.forEach { source ->
                if (Thread.currentThread().isInterrupted) {
                    return@forEach
                }
                runCatching {
                    RomScanner(this) { count, name ->
                        runOnUiThread {
                            if (generation == scanGeneration) {
                                nativeStatus.text = "${getString(R.string.native_version_label)}: Rescanning ${total + count} - $name"
                            }
                        }
                    }.scan(source)
                }
                    .onSuccess { roms ->
                        total += roms.size
                        libraryStore.mergeScan(source, roms)
                    }
                    .onFailure {
                        failed += 1
                    }
            }
            runOnUiThread {
                if (generation != scanGeneration) {
                    return@runOnUiThread
                }
                scanThread = null
                scanButton.text = "Scan Folder"
                updateRescanFoldersButton()
                updateLibraryFoldersButton()
                renderLibrary()
                val failedStatus = if (failed > 0) " ($failed failed)" else ""
                nativeStatus.text = "${getString(R.string.native_version_label)}: $total ROMs indexed$failedStatus"
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
        updateRescanFoldersButton()
        updateLibraryFoldersButton()
        nativeStatus.text = "${getString(R.string.native_version_label)}: Scan canceled"
    }

    private fun updateRescanFoldersButton() {
        val count = libraryStore.sourceFolders().size
        rescanFoldersButton.text = if (count > 0) "Rescan Folders ($count)" else "Rescan Folders"
        rescanFoldersButton.isEnabled = count > 0 || scanThread?.isAlive == true
    }

    private fun updateLibraryFoldersButton() {
        val count = libraryStore.sourceFolders().size
        libraryFoldersButton.text = if (count > 0) "Library Folders ($count)" else "Library Folders"
        libraryFoldersButton.isEnabled = count > 0
    }

    private fun showLibraryFoldersDialog() {
        val sources = libraryStore.sourceFolders()
        if (sources.isEmpty()) {
            updateLibraryFoldersButton()
            nativeStatus.text = "${getString(R.string.native_version_label)}: No folders added"
            return
        }
        val labels = sources.map { it.lastPathSegment ?: it.toString() }.toTypedArray()
        AlertDialog.Builder(this)
            .setTitle("Library Folders")
            .setItems(labels) { _, which ->
                confirmRemoveLibraryFolder(sources[which])
            }
            .setNeutralButton("Clear All") { _, _ ->
                confirmClearLibraryFolders(sources)
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun confirmRemoveLibraryFolder(source: Uri) {
        AlertDialog.Builder(this)
            .setTitle("Remove folder?")
            .setMessage(source.toString())
            .setPositiveButton("Remove") { _, _ ->
                val removed = libraryStore.removeSourceFolder(source)
                deleteCoverPaths(removed)
                releaseLibraryFolder(source)
                renderLibrary()
                updateRescanFoldersButton()
                updateLibraryFoldersButton()
                nativeStatus.text = "${getString(R.string.native_version_label)}: Folder removed (${removed.size} ROMs)"
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun confirmClearLibraryFolders(sources: List<Uri>) {
        AlertDialog.Builder(this)
            .setTitle("Clear library folders?")
            .setMessage("Remove all scanned folders and their indexed ROMs.")
            .setPositiveButton("Clear") { _, _ ->
                val removed = libraryStore.clearSourceFolders()
                deleteCoverPaths(removed)
                sources.forEach(::releaseLibraryFolder)
                renderLibrary()
                updateRescanFoldersButton()
                updateLibraryFoldersButton()
                nativeStatus.text = "${getString(R.string.native_version_label)}: Folders cleared (${removed.size} ROMs)"
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun releaseLibraryFolder(source: Uri) {
        runCatching {
            contentResolver.releasePersistableUriPermission(source, Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
    }

    private fun deleteCoverPaths(roms: List<LibraryRom>) {
        roms.map { it.coverPath }.distinct().forEach(::deleteCoverPath)
        coverThumbnailCache.evictAll()
    }

    private fun deleteCoverPath(path: String) {
        if (path.isBlank()) {
            return
        }
        val coversDir = File(filesDir, "covers").canonicalFile
        val file = runCatching { File(path).canonicalFile }.getOrNull() ?: return
        if (file.parentFile == coversDir) {
            file.delete()
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

    private fun openSettingsExportPicker() {
        val intent = Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "application/json"
            putExtra(Intent.EXTRA_TITLE, "mgba-settings.json")
            addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
        }
        startActivityForResult(intent, REQUEST_EXPORT_SETTINGS)
    }

    private fun openSettingsImportPicker() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        startActivityForResult(intent, REQUEST_IMPORT_SETTINGS)
    }

    private fun displayName(uri: Uri): String {
        if (uri.scheme == "file") {
            return uri.path?.let(::File)?.name?.takeIf { it.isNotBlank() } ?: uri.lastPathSegment ?: "rom"
        }
        var cursor: Cursor? = null
        return runCatching {
            cursor = contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)
            if (cursor != null && cursor.moveToFirst()) {
                cursor.getString(0) ?: uri.lastPathSegment ?: "rom"
            } else {
                uri.lastPathSegment ?: "rom"
            }
        }.getOrDefault(uri.lastPathSegment ?: "rom").also {
            cursor?.close()
        }
    }

    private fun sha1(value: String): String {
        val bytes = MessageDigest.getInstance("SHA-1").digest(value.toByteArray(Charsets.UTF_8))
        return bytes.joinToString("") { "%02x".format(it.toInt() and 0xff) }
    }

    private fun sha1(file: File): String {
        return runCatching {
            val digest = MessageDigest.getInstance("SHA-1")
            file.inputStream().use { input ->
                val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
                while (true) {
                    val read = input.read(buffer)
                    if (read <= 0) {
                        break
                    }
                    digest.update(buffer, 0, read)
                }
            }
            digest.digest().joinToString("") { "%02x".format(it.toInt() and 0xff) }
        }.getOrDefault("")
    }

    private fun sha1(uri: Uri): String {
        return runCatching {
            val digest = MessageDigest.getInstance("SHA-1")
            contentResolver.openInputStream(uri)?.use { input ->
                val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
                while (true) {
                    val read = input.read(buffer)
                    if (read <= 0) {
                        break
                    }
                    digest.update(buffer, 0, read)
                }
            } ?: return@runCatching ""
            digest.digest().joinToString("") { "%02x".format(it.toInt() and 0xff) }
        }.getOrDefault("")
    }

    private fun showAboutDialog() {
        val message = listOf(
            "Native core: ${NativeBridge.versionLabel()}",
            "mGBA copyright (c) 2013-2026 Jeffrey Pfau.",
            "mGBA is distributed under the Mozilla Public License 2.0.",
            "Android third-party notices are preserved in src/platform/android/THIRD_PARTY_NOTICES.md.",
            "No commercial ROMs or BIOS files are bundled.",
        ).joinToString(separator = "\n\n")
        AlertDialog.Builder(this)
            .setTitle("About mGBA")
            .setMessage(message)
            .setNeutralButton("Licenses") { _, _ ->
                showLicensesDialog()
            }
            .setPositiveButton("OK", null)
            .show()
    }

    private fun showLicensesDialog() {
        val message = listOf(
            "mGBA: Mozilla Public License 2.0.",
            "Android Gradle Plugin and Gradle wrapper: Apache License 2.0.",
            "zlib and MiniZip: zlib license.",
            "LZMA SDK: public domain by Igor Pavlov.",
            "inih: BSD 3-clause license.",
            "libpng source is preserved under the libpng license when enabled by a build.",
            "No AndroidX, Oboe, CameraX, Room, Flutter, React Native, Unity, Electron, or libretro runtime app dependency is currently bundled.",
            "No commercial ROMs or BIOS files are bundled.",
        ).joinToString(separator = "\n\n")
        AlertDialog.Builder(this)
            .setTitle("Licenses")
            .setMessage(message)
            .setPositiveButton("OK", null)
            .show()
    }

    private fun showCrashRecoveryPromptIfNeeded() {
        val processCrash = previousProcessCrashMessage()
        val markedCrash = AppLogStore.consumeCrashMarker(this)
        val message = markedCrash ?: processCrash ?: return
        nativeStatus.text = "${getString(R.string.native_version_label)}: Previous crash detected"
        AlertDialog.Builder(this)
            .setTitle("Previous Crash Detected")
            .setMessage("$message\n\nExport recent logs to Documents/mGBA for debugging?")
            .setPositiveButton("Export Logs") { _, _ ->
                exportLogs()
            }
            .setNegativeButton("Dismiss", null)
            .show()
    }

    private fun previousProcessCrashMessage(): String? {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return null
        }
        val manager = getSystemService(ActivityManager::class.java) ?: return null
        val exit = manager.getHistoricalProcessExitReasons(packageName, 0, 5)
            .firstOrNull { info ->
                isCrashExitReason(info.reason) &&
                    !AppLogStore.hasConsumedProcessExit(this, info.timestamp)
            } ?: return null
        AppLogStore.markProcessExitConsumed(this, exit.timestamp)
        return buildString {
            append("The previous process ended with ")
            append(exitReasonLabel(exit.reason))
            append(" at ")
            append(
                DateUtils.formatDateTime(
                    this@MainActivity,
                    exit.timestamp,
                    DateUtils.FORMAT_SHOW_DATE or DateUtils.FORMAT_SHOW_TIME,
                ),
            )
            val description = exit.description?.trim()
            if (!description.isNullOrEmpty()) {
                append(".\n")
                append(description)
            } else {
                append(".")
            }
        }
    }

    private fun isCrashExitReason(reason: Int): Boolean {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return false
        }
        return reason == ApplicationExitInfo.REASON_CRASH ||
            reason == ApplicationExitInfo.REASON_CRASH_NATIVE ||
            reason == ApplicationExitInfo.REASON_ANR
    }

    private fun exitReasonLabel(reason: Int): String {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return "a crash"
        }
        return when (reason) {
            ApplicationExitInfo.REASON_CRASH -> "a Java crash"
            ApplicationExitInfo.REASON_CRASH_NATIVE -> "a native crash"
            ApplicationExitInfo.REASON_ANR -> "an ANR"
            else -> "an unexpected exit"
        }
    }

    private fun exportLogs() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            openLogExportPicker()
            return
        }
        nativeStatus.text = "${getString(R.string.native_version_label)}: Exporting logs"
        Thread {
            val uri = LogExporter.exportRecent(this)
            runOnUiThread {
                if (uri == null) {
                    openLogExportPicker()
                } else {
                    nativeStatus.text = "${getString(R.string.native_version_label)}: Logs exported"
                }
            }
        }.start()
    }

    private fun openLogExportPicker() {
        val intent = Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "text/plain"
            putExtra(Intent.EXTRA_TITLE, LogExporter.recentLogFileName())
            addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
        }
        startActivityForResult(intent, REQUEST_EXPORT_LOGS)
    }

    private fun exportLogsToUri(uri: Uri) {
        nativeStatus.text = "${getString(R.string.native_version_label)}: Exporting logs"
        Thread {
            val ok = LogExporter.writeRecent(this, uri)
            runOnUiThread {
                nativeStatus.text = "${getString(R.string.native_version_label)}: ${if (ok) "Logs exported" else "Log export failed"}"
            }
        }.start()
    }

    private fun exportSettings(uri: Uri) {
        val ok = runCatching {
            contentResolver.openOutputStream(uri)?.bufferedWriter(Charsets.UTF_8)?.use { writer ->
                writer.write(settingsBackupJson())
            } != null
        }.getOrDefault(false)
        nativeStatus.text = "${getString(R.string.native_version_label)}: ${if (ok) "Settings exported" else "Settings export failed"}"
    }

    private fun importSettings(uri: Uri) {
        val raw = runCatching {
            contentResolver.openInputStream(uri)?.bufferedReader(Charsets.UTF_8)?.use { reader ->
                reader.readText()
            }
        }.getOrNull()
        val ok = raw?.let { importSettingsBackup(it) } == true
        if (ok) {
            updatePreferenceButtons()
            renderRecentGames()
            renderLibrary()
        }
        nativeStatus.text = "${getString(R.string.native_version_label)}: ${if (ok) "Settings imported" else "Settings import failed"}"
    }

    private fun settingsBackupJson(): String {
        return JSONObject()
            .put("version", 3)
            .put("emulatorPreferences", JSONObject(preferences.exportJson()))
            .put("perGameOverrides", perGameOverrides.exportJson())
            .put("inputMappings", inputMappingStore.exportJson())
            .put("romLibrary", libraryStore.exportJson())
            .put("recentGames", recentStore.exportJson())
            .toString(2)
    }

    private fun importSettingsBackup(raw: String): Boolean {
        val root = runCatching { JSONObject(raw) }.getOrNull() ?: return false
        val importedPreferences = preferences.importJson(raw)
        val overrides = root.optJSONObject("perGameOverrides")
        val importedOverrides = overrides?.let { perGameOverrides.importJson(it) } ?: true
        val mappings = root.optJSONObject("inputMappings")
        val importedMappings = mappings?.let { inputMappingStore.importJson(it) } ?: true
        val library = root.optJSONObject("romLibrary")
        val importedLibrary = library?.let { libraryStore.importJson(it) } ?: true
        val recentGames = root.optJSONArray("recentGames")
        val importedRecentGames = recentGames?.let { recentStore.importJson(it) } ?: true
        return importedPreferences && importedOverrides && importedMappings && importedLibrary && importedRecentGames
    }

    private fun clearArchiveCache() {
        val deleted = clearCacheDirectory("archive-roms") +
            clearCacheDirectory("archive-files") +
            clearCacheDirectory("imports")
        nativeStatus.text = "${getString(R.string.native_version_label)}: Cache cleared ($deleted files)"
    }

    private fun showStorageDialog() {
        AlertDialog.Builder(this)
            .setTitle("Storage")
            .setMessage(storageSummary())
            .setPositiveButton("OK", null)
            .setNegativeButton("Clear Logs") { _, _ ->
                val ok = AppLogStore.clear(this)
                nativeStatus.text = "${getString(R.string.native_version_label)}: ${if (ok) "Logs cleared" else "Log clear failed"}"
            }
            .setNeutralButton("Clear Cache") { _, _ ->
                clearArchiveCache()
            }
            .show()
    }

    private fun storageSummary(): String {
        val entries = listOf(
            "Saves" to File(filesDir, "saves"),
            "States" to File(filesDir, "states"),
            "State thumbnails" to File(filesDir, "state-thumbnails"),
            "Screenshots" to File(filesDir, "screenshots"),
            "Covers" to File(filesDir, "covers"),
            "Cheats" to File(filesDir, "cheats"),
            "Patches" to File(filesDir, "patches"),
            "BIOS" to File(filesDir, "bios"),
            "Logs" to File(filesDir, "logs"),
            "Archive cache" to File(cacheDir, "archive-roms"),
            "Archive files" to File(cacheDir, "archive-files"),
            "Import cache" to File(cacheDir, "imports"),
        )
        return entries.joinToString("\n") { (label, file) ->
            val stats = storageStats(file)
            "$label: ${stats.count} files, ${formatBytes(stats.bytes)}"
        }
    }

    private fun storageStats(file: File): StorageStats {
        if (!file.exists()) {
            return StorageStats(0, 0L)
        }
        if (file.isFile) {
            return StorageStats(1, file.length())
        }
        val children = file.listFiles().orEmpty()
        return children.fold(StorageStats(0, 0L)) { total, child ->
            val childStats = storageStats(child)
            StorageStats(total.count + childStats.count, total.bytes + childStats.bytes)
        }
    }

    private fun clearCacheDirectory(name: String): Int {
        val directory = File(cacheDir, name)
        return directory.listFiles()?.count { it.delete() } ?: 0
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

    private fun trimArchiveFileCache(keep: File? = null, maxBytes: Long = ARCHIVE_CACHE_MAX_BYTES) {
        val directory = File(cacheDir, "archive-files")
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

    private fun trimImportCache(keep: File? = null, maxBytes: Long = IMPORT_CACHE_MAX_BYTES) {
        val directory = File(cacheDir, "imports")
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
        private const val REQUEST_EXPORT_SETTINGS = 1006
        private const val REQUEST_IMPORT_SETTINGS = 1007
        private const val REQUEST_EXPORT_LOGS = 1008
        private const val MAX_LIBRARY_ITEMS = 24
        private const val LIBRARY_GRID_COLUMNS = 2
        private const val KEY_LIBRARY_VIEW_MODE = "libraryViewMode"
        private const val TRIM_MEMORY_RUNNING_LOW_LEVEL = 10
        private const val COVER_THUMBNAIL_CACHE_BYTES = 4 * 1024 * 1024
        private const val ARCHIVE_CACHE_MAX_BYTES = 256L * 1024L * 1024L
        private const val ARCHIVE_CACHE_TRIM_BYTES = 64L * 1024L * 1024L
        private const val IMPORT_CACHE_MAX_BYTES = 256L * 1024L * 1024L
        private val SCALE_LABELS = arrayOf("Fit", "Fill", "Integer", "Original", "Stretch")
        private val FILTER_LABELS = arrayOf("Pixel", "Smooth")
        private val FRAME_SKIP_LABELS = arrayOf("0", "1", "2", "3")
        private val ROM_ENTRY_EXTENSIONS = arrayOf(".gba", ".agb", ".gb", ".gbc", ".sgb")
    }
}

private enum class LibraryViewMode(val label: String) {
    List("List"),
    Grid("Grid");

    fun next(): LibraryViewMode {
        val modes = entries
        return modes[(ordinal + 1) % modes.size]
    }

    companion object {
        fun fromName(name: String?): LibraryViewMode {
            return entries.firstOrNull { it.name == name } ?: List
        }
    }
}

private enum class LibraryMode(val label: String) {
    All("All"),
    Favorites("Favorites"),
    Gba("GBA"),
    Gb("GB"),
    Gbc("GBC");

    fun next(): LibraryMode {
        val modes = entries
        return modes[(ordinal + 1) % modes.size]
    }

    fun matches(rom: LibraryRom): Boolean {
        return when (this) {
            All -> true
            Favorites -> rom.favorite
            Gba -> rom.hardwareLabel() == "GBA"
            Gb -> rom.hardwareLabel() == "GB"
            Gbc -> rom.hardwareLabel() == "GBC"
        }
    }

}

private data class StorageStats(
    val count: Int,
    val bytes: Long,
)

private data class RomHashes(
    val crc32: String = "",
    val sha1: String = "",
)

private fun LibraryRom.hardwareLabel(): String {
    val platform = this.platform.uppercase(java.util.Locale.US)
    val system = this.system.uppercase(java.util.Locale.US)
    return when {
        platform == "GBA" || system == "AGB" -> "GBA"
        system == "CGB" || displayName.endsWith(".gbc", ignoreCase = true) -> "GBC"
        platform == "GB" || system == "DMG" -> "GB"
        platform.isNotBlank() -> platform
        else -> system
    }
}
