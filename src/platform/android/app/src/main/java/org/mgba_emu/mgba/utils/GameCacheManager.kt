
package org.mgba_emu.mgba.utils

import android.content.Context
import android.net.Uri
import org.mgba_emu.mgba.model.GameModel
import org.json.JSONObject
import androidx.core.content.edit
import org.mgba_emu.mgba.mGBAApplication

object GameCacheManager {
    private val prefs = mGBAApplication.context.getSharedPreferences("games_cache", Context.MODE_PRIVATE)

    fun getGame(uri: Uri, fileName: String): GameModel? {
        val jsonString = prefs.getString(uri.toString(), null) ?: return null

        return try {
            val json = JSONObject(jsonString)
            GameModel(
                uri = uri,
                fileName = fileName,
                title = json.optString("title").takeIf { it.isNotEmpty() },
                version = json.optString("version").takeIf { it.isNotEmpty() },
                iconUrl = json.optString("iconUrl").takeIf { it.isNotEmpty() },
                lastPlayed = json.optLong("lastPlayed", 0L)
            )
        } catch (_: Exception) {
            null
        }
    }

    fun saveGame(game: GameModel) {
        val json = JSONObject().apply {
            put("title", game.title ?: "")
            put("version", game.version ?: "")
            put("iconUrl", game.iconUrl ?: "")
            put("lastPlayed", game.lastPlayed)
        }

        prefs.edit { putString(game.uri.toString(), json.toString()) }
    }

    fun getAllCachedGames(): List<GameModel> {
        val allGames = mutableListOf<GameModel>()
        val allEntries = prefs.all

        for ((uriString, jsonString) in allEntries) {
            if (jsonString is String) {
                try {
                    val json = JSONObject(jsonString)
                    val uri = Uri.parse(uriString)
                    val FallbackName = uri.lastPathSegment ?: "Unknown Game"

                    allGames.add(
                        GameModel(
                            uri = uri,
                            fileName = FallbackName,
                            title = json.optString("title").takeIf { it.isNotEmpty() },
                            version = json.optString("version").takeIf { it.isNotEmpty() },
                            iconUrl = json.optString("iconUrl").takeIf { it.isNotEmpty() },
                            lastPlayed = json.optLong("lastPlayed", 0L)
                        )
                    )
                } catch (_: Exception) {}
            }
        }
        return allGames
    }
}
