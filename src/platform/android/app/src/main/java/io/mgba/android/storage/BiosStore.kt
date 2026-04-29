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
    }
}
