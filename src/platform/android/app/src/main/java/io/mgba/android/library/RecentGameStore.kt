package io.mgba.android.library

import android.content.Context
import android.net.Uri
import org.json.JSONArray
import org.json.JSONObject

data class RecentGame(
    val uri: Uri,
    val displayName: String,
    val lastOpened: Long,
)

class RecentGameStore(context: Context) {
    private val preferences = context.applicationContext.getSharedPreferences("recent_games", Context.MODE_PRIVATE)

    fun list(): List<RecentGame> {
        val json = preferences.getString(KEY_ITEMS, "[]") ?: "[]"
        val array = runCatching { JSONArray(json) }.getOrDefault(JSONArray())
        return buildList {
            for (index in 0 until array.length()) {
                val item = array.optJSONObject(index) ?: continue
                val uri = item.optString("uri").takeIf { it.isNotBlank() } ?: continue
                val displayName = item.optString("displayName", uri)
                add(
                    RecentGame(
                        uri = Uri.parse(uri),
                        displayName = displayName,
                        lastOpened = item.optLong("lastOpened", 0L),
                    ),
                )
            }
        }.sortedByDescending { it.lastOpened }
    }

    fun add(uri: Uri, displayName: String) {
        val existing = list().filterNot { it.uri == uri }
        val updated = listOf(
            RecentGame(uri, displayName, System.currentTimeMillis()),
        ) + existing
        val array = JSONArray()
        updated.take(MAX_ITEMS).forEach { item ->
            array.put(
                JSONObject()
                    .put("uri", item.uri.toString())
                    .put("displayName", item.displayName)
                    .put("lastOpened", item.lastOpened),
            )
        }
        preferences.edit().putString(KEY_ITEMS, array.toString()).apply()
    }

    private companion object {
        const val KEY_ITEMS = "items"
        const val MAX_ITEMS = 8
    }
}
