package io.mgba.android.storage

import android.content.Context
import android.net.Uri
import java.io.File
import java.security.MessageDigest

data class BiosInfo(
    val displayName: String,
    val sizeBytes: Long,
    val sha1: String,
)

class BiosStore(context: Context) {
    private val appContext = context.applicationContext
    private val preferences = appContext.getSharedPreferences("bios", Context.MODE_PRIVATE)

    val displayName: String?
        get() = preferences.getString(KEY_DISPLAY_NAME, null)

    val info: BiosInfo?
        get() {
            val file = defaultFile().takeIf { it.isFile } ?: return null
            return BiosInfo(
                displayName = displayName ?: file.name,
                sizeBytes = file.length(),
                sha1 = sha1(file),
            )
        }

    fun importDefault(uri: Uri, displayName: String): Boolean {
        val directory = biosDirectory() ?: return false
        val target = defaultFile(directory)
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
            preferences.edit().putString(KEY_DISPLAY_NAME, displayName).apply()
            true
        }.getOrDefault(false)
    }

    fun clearDefault(): Boolean {
        val deleted = runCatching {
            val file = defaultFile()
            !file.exists() || file.delete()
        }.getOrDefault(false)
        if (deleted) {
            preferences.edit().remove(KEY_DISPLAY_NAME).apply()
        }
        return deleted
    }

    private fun biosDirectory(): File? {
        val directory = File(appContext.filesDir, BIOS_DIRECTORY)
        return if (directory.exists() || directory.mkdirs()) directory else null
    }

    private fun defaultFile(directory: File = File(appContext.filesDir, BIOS_DIRECTORY)): File {
        return File(directory, DEFAULT_BIOS_NAME)
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
        private const val KEY_DISPLAY_NAME = "displayName"
        private const val BIOS_DIRECTORY = "bios"
        private const val DEFAULT_BIOS_NAME = "default.bios"
    }
}
