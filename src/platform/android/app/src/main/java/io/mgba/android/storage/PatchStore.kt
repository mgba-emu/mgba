package io.mgba.android.storage

import android.content.Context
import android.net.Uri
import java.io.File
import java.security.MessageDigest

class PatchStore(context: Context) {
    private val appContext = context.applicationContext
    private val preferences = appContext.getSharedPreferences("patch", Context.MODE_PRIVATE)

    val displayName: String?
        get() = preferences.getString(KEY_DISPLAY_NAME, null)

    fun importDefault(uri: Uri, displayName: String): Boolean {
        val directory = File(appContext.filesDir, PATCH_DIRECTORY)
        if (!directory.exists() && !directory.mkdirs()) {
            return false
        }
        val target = File(directory, DEFAULT_PATCH_NAME)
        val imported = replaceFileAtomically(target) { temp ->
            val input = appContext.contentResolver.openInputStream(uri) ?: error("Patch input unavailable")
            input.use {
                temp.outputStream().use { output ->
                    it.copyTo(output)
                }
            }
        }
        if (imported) {
            preferences.edit().putString(KEY_DISPLAY_NAME, displayName).apply()
        }
        return imported
    }

    fun importForGame(gameId: String?, uri: Uri, displayName: String): Boolean {
        val id = patchId(gameId) ?: return false
        val directory = patchDirectory() ?: return false
        val target = File(directory, "$id${patchExtension(displayName)}")
        val imported = replaceFileAtomically(target) { temp ->
            val input = appContext.contentResolver.openInputStream(uri) ?: error("Patch input unavailable")
            input.use {
                temp.outputStream().use { output ->
                    it.copyTo(output)
                }
            }
        }
        if (imported) {
            preferences.edit()
                .putString(gameDisplayNameKey(id), displayName)
                .putString(gameFileNameKey(id), target.name)
                .apply()
        }
        return imported
    }

    fun importForGameFile(gameId: String?, file: File, displayName: String): Boolean {
        val id = patchId(gameId) ?: return false
        val directory = patchDirectory() ?: return false
        val target = File(directory, "$id${patchExtension(displayName)}")
        val imported = replaceFileAtomically(target) { temp ->
            file.copyTo(temp, overwrite = true)
        }
        if (imported) {
            preferences.edit()
                .putString(gameDisplayNameKey(id), displayName)
                .putString(gameFileNameKey(id), target.name)
                .apply()
        }
        return imported
    }

    fun fileForGame(gameId: String?): File? {
        val id = patchId(gameId) ?: return null
        val fileName = preferences.getString(gameFileNameKey(id), null) ?: return null
        val file = File(File(appContext.filesDir, PATCH_DIRECTORY), fileName)
        return file.takeIf { it.isFile }
    }

    fun autoPatchFile(displayName: String, crc32: String): File? {
        val directory = File(appContext.filesDir, PATCH_DIRECTORY)
        if (!directory.isDirectory) {
            return null
        }
        val normalizedCrc = crc32.trim().lowercase().takeIf { it.isNotBlank() }
        val displayBase = displayName.substringBeforeLast('.', displayName).trim().takeIf { it.isNotBlank() }
        val displayFile = displayName.trim().takeIf { it.isNotBlank() }
        val names = buildList {
            listOfNotNull(displayBase, displayFile).distinct().forEach { base ->
                PATCH_EXTENSIONS.forEach { extension -> add("$base.$extension") }
            }
            normalizedCrc?.let { hash ->
                PATCH_EXTENSIONS.forEach { extension -> add("$hash.$extension") }
            }
        }.distinct()
        return names
            .asSequence()
            .map { File(directory, it) }
            .firstOrNull { it.isFile }
    }

    fun displayNameForGame(gameId: String?): String? {
        val id = patchId(gameId) ?: return null
        return preferences.getString(gameDisplayNameKey(id), null)
    }

    fun clearForGame(gameId: String?): Boolean {
        val id = patchId(gameId) ?: return false
        val fileName = preferences.getString(gameFileNameKey(id), null)
        val deleted = runCatching {
            if (fileName == null) {
                true
            } else {
                val file = File(File(appContext.filesDir, PATCH_DIRECTORY), fileName)
                !file.exists() || file.delete()
            }
        }.getOrDefault(false)
        if (deleted) {
            preferences.edit()
                .remove(gameDisplayNameKey(id))
                .remove(gameFileNameKey(id))
                .apply()
        }
        return deleted
    }

    private fun patchDirectory(): File? {
        val directory = File(appContext.filesDir, PATCH_DIRECTORY)
        return if (directory.exists() || directory.mkdirs()) directory else null
    }

    private fun patchId(gameId: String?): String? {
        val value = gameId?.takeIf { it.isNotBlank() } ?: return null
        val digest = MessageDigest.getInstance("SHA-1").digest(value.toByteArray(Charsets.UTF_8))
        return digest.joinToString("") { "%02x".format(it.toInt() and 0xFF) }
    }

    private fun patchExtension(displayName: String): String {
        val lower = displayName.substringAfterLast('.', "").lowercase()
        return when (lower) {
            "ips", "ups", "bps" -> ".$lower"
            else -> ".patch"
        }
    }

    private fun gameDisplayNameKey(id: String): String {
        return "$KEY_GAME_DISPLAY_NAME_PREFIX$id"
    }

    private fun gameFileNameKey(id: String): String {
        return "$KEY_GAME_FILE_NAME_PREFIX$id"
    }

    companion object {
        private const val KEY_DISPLAY_NAME = "displayName"
        private const val KEY_GAME_DISPLAY_NAME_PREFIX = "gameDisplayName:"
        private const val KEY_GAME_FILE_NAME_PREFIX = "gameFileName:"
        private const val PATCH_DIRECTORY = "patches"
        private const val DEFAULT_PATCH_NAME = "default.patch"
        private val PATCH_EXTENSIONS = arrayOf("ips", "ups", "bps")
    }
}
