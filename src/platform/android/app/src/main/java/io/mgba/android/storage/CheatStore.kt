package io.mgba.android.storage

import android.content.Context
import android.net.Uri
import java.io.File
import java.security.MessageDigest

class CheatStore(context: Context) {
    private val appContext = context.applicationContext
    private val preferences = appContext.getSharedPreferences("cheats", Context.MODE_PRIVATE)

    fun importForGame(gameId: String?, uri: Uri, displayName: String): Boolean {
        val id = cheatId(gameId) ?: return false
        val directory = cheatDirectory() ?: return false
        val target = File(directory, "$id.cheats")
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
            preferences.edit()
                .putString(gameDisplayNameKey(id), displayName)
                .putString(gameFileNameKey(id), target.name)
                .apply()
            true
        }.getOrDefault(false)
    }

    fun fileForGame(gameId: String?): File? {
        val id = cheatId(gameId) ?: return null
        val fileName = preferences.getString(gameFileNameKey(id), null) ?: return null
        val file = File(File(appContext.filesDir, CHEAT_DIRECTORY), fileName)
        return file.takeIf { it.isFile }
    }

    fun displayNameForGame(gameId: String?): String? {
        val id = cheatId(gameId) ?: return null
        return preferences.getString(gameDisplayNameKey(id), null)
    }

    private fun cheatDirectory(): File? {
        val directory = File(appContext.filesDir, CHEAT_DIRECTORY)
        return if (directory.exists() || directory.mkdirs()) directory else null
    }

    private fun cheatId(gameId: String?): String? {
        val value = gameId?.takeIf { it.isNotBlank() } ?: return null
        val digest = MessageDigest.getInstance("SHA-1").digest(value.toByteArray(Charsets.UTF_8))
        return digest.joinToString("") { "%02x".format(it.toInt() and 0xFF) }
    }

    private fun gameDisplayNameKey(id: String): String {
        return "$KEY_GAME_DISPLAY_NAME_PREFIX$id"
    }

    private fun gameFileNameKey(id: String): String {
        return "$KEY_GAME_FILE_NAME_PREFIX$id"
    }

    companion object {
        private const val KEY_GAME_DISPLAY_NAME_PREFIX = "gameDisplayName:"
        private const val KEY_GAME_FILE_NAME_PREFIX = "gameFileName:"
        private const val CHEAT_DIRECTORY = "cheats"
    }
}
