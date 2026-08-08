
package org.mgba_emu.mgba.utils

import android.content.Context
import android.content.SharedPreferences
import android.net.Uri
import org.mgba_emu.mgba.mGBAApplication
import androidx.core.content.edit

object SearchLocationHelper {
    private val prefs: SharedPreferences by lazy {
        mGBAApplication.context.getSharedPreferences("app_prefs", Context.MODE_PRIVATE)
    }
    private const val GAME_FOLDERS = "game_folders"

    fun saveFolderUri(uri: Uri) {
        val savedUris = prefs.getStringSet(GAME_FOLDERS, mutableSetOf())?.toMutableSet() ?: mutableSetOf()
        savedUris.add(uri.toString())
        prefs.edit { putStringSet(GAME_FOLDERS, savedUris) }
    }

    fun removeFolder(uri: Uri) {
        val savedUris = prefs.getStringSet(GAME_FOLDERS, mutableSetOf())?.toMutableSet() ?: mutableSetOf()
        savedUris.remove(uri.toString())
        prefs.edit { putStringSet(GAME_FOLDERS, savedUris) }
    }

    fun getGameFolders(): List<Uri> {
        val savedUris = prefs.getStringSet(GAME_FOLDERS, emptySet()) ?: emptySet()
        return savedUris.map { Uri.parse(it) }
    }

    fun isFolderExists(folder: Uri): Boolean {
        return getGameFolders().contains(folder)
    }
}
