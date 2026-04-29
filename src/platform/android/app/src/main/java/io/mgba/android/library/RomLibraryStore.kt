package io.mgba.android.library

import android.content.Context
import android.net.Uri
import org.json.JSONArray
import org.json.JSONObject

data class LibraryRom(
    val uri: Uri,
    val displayName: String,
    val title: String = "",
    val platform: String = "",
    val crc32: String = "",
    val lastPlayedAt: Long = 0L,
    val favorite: Boolean = false,
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
                        title = item.optString("title"),
                        platform = item.optString("platform"),
                        crc32 = item.optString("crc32"),
                        lastPlayedAt = item.optLong("lastPlayedAt", 0L),
                        favorite = item.optBoolean("favorite", false),
                    ),
                )
            }
        }.sortedWith(librarySort)
    }

    fun replace(items: List<LibraryRom>) {
        val existing = list().associateBy { it.uri }
        val array = JSONArray()
        items.distinctBy { it.uri }.sortedWith(librarySort).forEach { item ->
            val previous = existing[item.uri]
            val merged = item.copy(
                title = item.title.ifBlank { previous?.title.orEmpty() },
                platform = item.platform.ifBlank { previous?.platform.orEmpty() },
                crc32 = item.crc32.ifBlank { previous?.crc32.orEmpty() },
                lastPlayedAt = previous?.lastPlayedAt ?: item.lastPlayedAt,
                favorite = previous?.favorite ?: item.favorite,
            )
            array.put(
                toJson(merged),
            )
        }
        preferences.edit().putString(KEY_ITEMS, array.toString()).apply()
    }

    fun markPlayed(uri: Uri, playedAt: Long = System.currentTimeMillis()) {
        val array = JSONArray()
        list().forEach { item ->
            val updated = if (item.uri == uri) item.copy(lastPlayedAt = playedAt) else item
            array.put(toJson(updated))
        }
        preferences.edit().putString(KEY_ITEMS, array.toString()).apply()
    }

    fun toggleFavorite(uri: Uri) {
        val array = JSONArray()
        list().forEach { item ->
            val updated = if (item.uri == uri) item.copy(favorite = !item.favorite) else item
            array.put(toJson(updated))
        }
        preferences.edit().putString(KEY_ITEMS, array.toString()).apply()
    }

    fun remove(uri: Uri) {
        val array = JSONArray()
        list().filterNot { it.uri == uri }.forEach { item ->
            array.put(toJson(item))
        }
        preferences.edit().putString(KEY_ITEMS, array.toString()).apply()
    }

    private fun toJson(item: LibraryRom): JSONObject {
        return JSONObject()
            .put("uri", item.uri.toString())
            .put("displayName", item.displayName)
            .put("title", item.title)
            .put("platform", item.platform)
            .put("crc32", item.crc32)
            .put("lastPlayedAt", item.lastPlayedAt)
            .put("favorite", item.favorite)
    }

    private companion object {
        const val KEY_ITEMS = "items"
        val librarySort = compareByDescending<LibraryRom> { it.favorite }.thenBy { it.displayName.lowercase() }
    }
}
