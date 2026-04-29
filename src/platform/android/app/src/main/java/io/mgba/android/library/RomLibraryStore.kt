package io.mgba.android.library

import android.content.Context
import android.net.Uri
import org.json.JSONArray
import org.json.JSONObject

data class LibraryRom(
    val uri: Uri,
    val displayName: String,
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
                    ),
                )
            }
        }.sortedBy { it.displayName.lowercase() }
    }

    fun replace(items: List<LibraryRom>) {
        val array = JSONArray()
        items.distinctBy { it.uri }.sortedBy { it.displayName.lowercase() }.forEach { item ->
            array.put(
                JSONObject()
                    .put("uri", item.uri.toString())
                    .put("displayName", item.displayName),
            )
        }
        preferences.edit().putString(KEY_ITEMS, array.toString()).apply()
    }

    private companion object {
        const val KEY_ITEMS = "items"
    }
}
