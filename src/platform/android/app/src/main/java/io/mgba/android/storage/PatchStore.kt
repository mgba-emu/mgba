package io.mgba.android.storage

import android.content.Context
import android.net.Uri
import java.io.File

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
        private const val PATCH_DIRECTORY = "patches"
        private const val DEFAULT_PATCH_NAME = "default.patch"
    }
}
