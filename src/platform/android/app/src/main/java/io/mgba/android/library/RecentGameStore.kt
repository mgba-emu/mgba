package io.mgba.android.library

import android.content.Context
import android.net.Uri
import org.json.JSONArray
import org.json.JSONObject

data class RecentGame(
    val uri: Uri,
    val displayName: String,
    val lastOpened: Long,
    val stableId: String = "",
    val crc32: String = "",
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
                        stableId = item.optString("stableId"),
                        crc32 = item.optString("crc32"),
                    ),
                )
            }
        }.sortedByDescending { it.lastOpened }
    }

    fun add(uri: Uri, displayName: String, stableId: String = "", crc32: String = "") {
        val existing = list().filterNot { it.uri == uri }
        val updated = listOf(
            RecentGame(uri, displayName, System.currentTimeMillis(), stableId, crc32),
        ) + existing
        write(updated)
    }

    fun clear(): Int {
        val count = list().size
        preferences.edit().putString(KEY_ITEMS, JSONArray().toString()).apply()
        return count
    }

    fun exportJson(): JSONArray {
        val array = JSONArray()
        list().forEach { item -> array.put(toJson(item)) }
        return array
    }

    fun importJson(array: JSONArray): Boolean {
        val items = buildList {
            for (index in 0 until array.length()) {
                jsonToRecent(array.optJSONObject(index))?.let(::add)
            }
        }
        write(items)
        return true
    }

    private fun write(items: List<RecentGame>) {
        val array = JSONArray()
        items.take(MAX_ITEMS).forEach { item -> array.put(toJson(item)) }
        preferences.edit().putString(KEY_ITEMS, array.toString()).apply()
    }

    private fun toJson(item: RecentGame): JSONObject {
        return JSONObject()
            .put("uri", item.uri.toString())
            .put("displayName", item.displayName)
            .put("lastOpened", item.lastOpened)
            .put("stableId", item.stableId)
            .put("crc32", item.crc32)
    }

    private fun jsonToRecent(item: JSONObject?): RecentGame? {
        if (item == null) {
            return null
        }
        val uri = item.optString("uri").takeIf { it.isNotBlank() } ?: return null
        return RecentGame(
            uri = Uri.parse(uri),
            displayName = item.optString("displayName", uri),
            lastOpened = item.optLong("lastOpened", 0L),
            stableId = item.optString("stableId"),
            crc32 = item.optString("crc32"),
        )
    }

    private companion object {
        const val KEY_ITEMS = "items"
        const val MAX_ITEMS = 8
    }
}
