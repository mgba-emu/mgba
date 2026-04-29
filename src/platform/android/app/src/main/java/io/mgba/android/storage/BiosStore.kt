package io.mgba.android.storage

import android.content.Context
import android.net.Uri
import java.io.File

class BiosStore(context: Context) {
    private val appContext = context.applicationContext
    private val preferences = appContext.getSharedPreferences("bios", Context.MODE_PRIVATE)

    val displayName: String?
        get() = preferences.getString(KEY_DISPLAY_NAME, null)

    fun importDefault(uri: Uri, displayName: String): Boolean {
        val directory = File(appContext.filesDir, BIOS_DIRECTORY)
        if (!directory.exists() && !directory.mkdirs()) {
            return false
        }
        val target = File(directory, DEFAULT_BIOS_NAME)
        return runCatching {
            appContext.contentResolver.openInputStream(uri)?.use { input ->
                target.outputStream().use { output ->
                    input.copyTo(output)
                }
            } ?: return false
            preferences.edit().putString(KEY_DISPLAY_NAME, displayName).apply()
            true
        }.getOrDefault(false)
    }

    companion object {
        private const val KEY_DISPLAY_NAME = "displayName"
        private const val BIOS_DIRECTORY = "bios"
        private const val DEFAULT_BIOS_NAME = "default.bios"
    }
}
