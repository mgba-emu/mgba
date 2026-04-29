package io.mgba.android.storage

import android.content.Context
import android.net.Uri
import java.io.File
import java.security.MessageDigest

data class BiosInfo(
    val slot: BiosSlot,
    val displayName: String,
    val sizeBytes: Long,
    val sha1: String,
)

enum class BiosSlot(val label: String, val fileName: String, val displayNameKey: String) {
    Default("Default", "default.bios", "displayName:default"),
    Gba("GBA", "gba.bios", "displayName:gba"),
    Gb("GB", "gb.bios", "displayName:gb"),
    Gbc("GBC", "gbc.bios", "displayName:gbc"),
}

class BiosStore(context: Context) {
    private val appContext = context.applicationContext
    private val preferences = appContext.getSharedPreferences("bios", Context.MODE_PRIVATE)

    val displayName: String?
        get() = storedDisplayName(BiosSlot.Default)

    val info: BiosInfo?
        get() = info(BiosSlot.Default)

    val infos: List<BiosInfo>
        get() = BiosSlot.entries.mapNotNull { info(it) }

    fun info(slot: BiosSlot): BiosInfo? {
        val file = biosFile(slot).takeIf { it.isFile } ?: return null
        return BiosInfo(
            slot = slot,
            displayName = storedDisplayName(slot) ?: file.name,
            sizeBytes = file.length(),
            sha1 = sha1(file),
        )
    }

    fun importDefault(uri: Uri, displayName: String): Boolean {
        return import(BiosSlot.Default, uri, displayName)
    }

    fun import(slot: BiosSlot, uri: Uri, displayName: String): Boolean {
        val directory = biosDirectory() ?: return false
        val target = biosFile(slot, directory)
        val tmp = File(directory, "${target.name}.tmp")
        return runCatching {
            appContext.contentResolver.openInputStream(uri)?.use { input ->
                tmp.outputStream().use { output ->
                    input.copyTo(output)
                }
            } ?: return false
            if (target.exists()) {
                target.delete()
            }
            if (!tmp.renameTo(target)) {
                tmp.delete()
                return false
            }
            preferences.edit().putString(slot.displayNameKey, displayName).apply()
            true
        }.getOrDefault(false)
    }

    fun importForGame(gameId: String?, slot: BiosSlot, uri: Uri, displayName: String): Boolean {
        val directory = gameBiosDirectory(gameId) ?: return false
        val target = biosFile(slot, directory)
        val tmp = File(directory, "${target.name}.tmp")
        return runCatching {
            appContext.contentResolver.openInputStream(uri)?.use { input ->
                tmp.outputStream().use { output ->
                    input.copyTo(output)
                }
            } ?: return false
            if (target.exists()) {
                target.delete()
            }
            if (!tmp.renameTo(target)) {
                tmp.delete()
                return false
            }
            preferences.edit().putString(gameDisplayNameKey(gameId, slot), displayName).apply()
            true
        }.getOrDefault(false)
    }

    fun importForGameFile(gameId: String?, slot: BiosSlot, file: File, displayName: String): Boolean {
        val directory = gameBiosDirectory(gameId) ?: return false
        val target = biosFile(slot, directory)
        return runCatching {
            file.copyTo(target, overwrite = true)
            preferences.edit().putString(gameDisplayNameKey(gameId, slot), displayName).apply()
            true
        }.getOrDefault(false)
    }

    fun clearDefault(): Boolean {
        return clear(BiosSlot.Default)
    }

    fun clear(slot: BiosSlot): Boolean {
        val deleted = runCatching {
            val file = biosFile(slot)
            !file.exists() || file.delete()
        }.getOrDefault(false)
        if (deleted) {
            preferences.edit()
                .remove(slot.displayNameKey)
                .apply {
                    if (slot == BiosSlot.Default) {
                        remove(KEY_LEGACY_DISPLAY_NAME)
                    }
                }
                .apply()
        }
        return deleted
    }

    fun clearForGame(gameId: String?, slot: BiosSlot): Boolean {
        val deleted = runCatching {
            val file = fileForGame(gameId, slot) ?: return@runCatching true
            !file.exists() || file.delete()
        }.getOrDefault(false)
        if (deleted) {
            preferences.edit()
                .remove(gameDisplayNameKey(gameId, slot))
                .apply()
        }
        return deleted
    }

    fun infoForGame(gameId: String?, slot: BiosSlot): BiosInfo? {
        val file = fileForGame(gameId, slot)?.takeIf { it.isFile } ?: return null
        return BiosInfo(
            slot = slot,
            displayName = preferences.getString(gameDisplayNameKey(gameId, slot), null) ?: file.name,
            sizeBytes = file.length(),
            sha1 = sha1(file),
        )
    }

    fun infosForGame(gameId: String?): List<BiosInfo> {
        return BiosSlot.entries.mapNotNull { infoForGame(gameId, it) }
    }

    fun fileForGame(gameId: String?, slot: BiosSlot): File? {
        val directory = gameBiosDirectory(gameId, create = false) ?: return null
        return biosFile(slot, directory)
    }

    fun pathForGame(gameId: String?, slot: BiosSlot): String {
        return fileForGame(gameId, slot)?.takeIf { it.isFile }?.absolutePath.orEmpty()
    }

    fun migrateGameId(primaryGameId: String?, legacyGameId: String?): Boolean {
        val primaryDirectory = gameBiosDirectory(primaryGameId) ?: return false
        val legacyDirectory = gameBiosDirectory(legacyGameId, create = false) ?: return false
        if (primaryDirectory == legacyDirectory || !legacyDirectory.isDirectory) {
            return false
        }
        var changed = false
        val editor = preferences.edit()
        BiosSlot.entries.forEach { slot ->
            val source = biosFile(slot, legacyDirectory)
            val target = biosFile(slot, primaryDirectory)
            if (source.isFile && !target.exists()) {
                source.copyTo(target, overwrite = false)
                preferences.getString(gameDisplayNameKey(legacyGameId, slot), null)?.let { displayName ->
                    editor.putString(gameDisplayNameKey(primaryGameId, slot), displayName)
                }
                changed = true
            }
        }
        if (changed) {
            editor.apply()
        }
        return changed
    }

    private fun storedDisplayName(slot: BiosSlot): String? {
        return preferences.getString(slot.displayNameKey, null)
            ?: if (slot == BiosSlot.Default) preferences.getString(KEY_LEGACY_DISPLAY_NAME, null) else null
    }

    private fun biosDirectory(): File? {
        val directory = File(appContext.filesDir, BIOS_DIRECTORY)
        return if (directory.exists() || directory.mkdirs()) directory else null
    }

    private fun biosFile(slot: BiosSlot, directory: File = File(appContext.filesDir, BIOS_DIRECTORY)): File {
        return File(directory, slot.fileName)
    }

    private fun gameBiosDirectory(gameId: String?, create: Boolean = true): File? {
        val id = gameBiosId(gameId) ?: return null
        val directory = File(File(appContext.filesDir, "$BIOS_DIRECTORY/games"), id)
        return when {
            directory.isDirectory -> directory
            create && directory.mkdirs() -> directory
            else -> null
        }
    }

    private fun gameDisplayNameKey(gameId: String?, slot: BiosSlot): String {
        val id = gameBiosId(gameId).orEmpty()
        return "$KEY_GAME_DISPLAY_NAME_PREFIX$id:${slot.name}"
    }

    private fun gameBiosId(gameId: String?): String? {
        val value = gameId?.takeIf { it.isNotBlank() } ?: return null
        val digest = MessageDigest.getInstance("SHA-1").digest(value.toByteArray(Charsets.UTF_8))
        return digest.joinToString("") { "%02x".format(it.toInt() and 0xFF) }
    }

    private fun sha1(file: File): String {
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
        return digest.digest().joinToString("") { "%02x".format(it.toInt() and 0xFF) }
    }

    companion object {
        private const val BIOS_DIRECTORY = "bios"
        private const val KEY_LEGACY_DISPLAY_NAME = "displayName"
        private const val KEY_GAME_DISPLAY_NAME_PREFIX = "gameDisplayName:"
    }
}
