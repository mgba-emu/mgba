package io.mgba.android.library

import android.content.Context
import android.net.Uri
import org.json.JSONArray
import org.json.JSONObject

data class LibraryRom(
    val uri: Uri,
    val displayName: String,
    val lastPlayedAt: Long = 0L,
)

class RomLibraryStore(context: Context) {
    private val preferences = context.applicationContext.getSharedPreferences("rom_library", Context.MODE_PRIVATE)

    fun list(): List<LibraryRom> {
        val json = preferences.getString(KEY_ITEMS, "[]") ?: "[]"
        val array = runCatching { JSONArray(json) }.getOrDefault(JSONArray())
        return buildList {
            for (index in 0 until array.length()) {
                val item = array.optJSONObject(index) ?: continue
                val uri = item.optString("uri").takeIf { it.isNotBlank() } ?: continue
                add(
                    LibraryRom(
                        uri = Uri.parse(uri),
                        displayName = item.optString("displayName", uri),
                        lastPlayedAt = item.optLong("lastPlayedAt", 0L),
                    ),
                )
            }
        }.sortedBy { it.displayName.lowercase() }
    }

    fun replace(items: List<LibraryRom>) {
        val existing = list().associateBy { it.uri }
        val array = JSONArray()
        items.distinctBy { it.uri }.sortedBy { it.displayName.lowercase() }.forEach { item ->
            val merged = item.copy(lastPlayedAt = existing[item.uri]?.lastPlayedAt ?: item.lastPlayedAt)
            array.put(
                JSONObject()
                    .put("uri", merged.uri.toString())
                    .put("displayName", merged.displayName)
                    .put("lastPlayedAt", merged.lastPlayedAt),
            )
        }
        preferences.edit().putString(KEY_ITEMS, array.toString()).apply()
    }

    fun markPlayed(uri: Uri, playedAt: Long = System.currentTimeMillis()) {
        val array = JSONArray()
        list().forEach { item ->
            val updated = if (item.uri == uri) item.copy(lastPlayedAt = playedAt) else item
            array.put(
                JSONObject()
                    .put("uri", updated.uri.toString())
                    .put("displayName", updated.displayName)
                    .put("lastPlayedAt", updated.lastPlayedAt),
            )
        }
        preferences.edit().putString(KEY_ITEMS, array.toString()).apply()
    }

    private companion object {
        const val KEY_ITEMS = "items"
    }
}
