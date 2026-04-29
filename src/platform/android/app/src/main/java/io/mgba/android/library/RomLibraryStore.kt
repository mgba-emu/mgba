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
    val system: String = "",
    val gameCode: String = "",
    val maker: String = "",
    val version: Int = -1,
    val crc32: String = "",
    val sha1: String = "",
    val fileSize: Long = 0L,
    val lastPlayedAt: Long = 0L,
    val playTimeSeconds: Long = 0L,
    val favorite: Boolean = false,
    val coverPath: String = "",
    val sourceTreeUri: Uri? = null,
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
                        system = item.optString("system"),
                        gameCode = item.optString("gameCode"),
                        maker = item.optString("maker"),
                        version = item.optInt("version", -1),
                        crc32 = item.optString("crc32"),
                        sha1 = item.optString("sha1"),
                        fileSize = item.optLong("fileSize", 0L),
                        lastPlayedAt = item.optLong("lastPlayedAt", 0L),
                        playTimeSeconds = item.optLong("playTimeSeconds", 0L),
                        favorite = item.optBoolean("favorite", false),
                        coverPath = item.optString("coverPath"),
                        sourceTreeUri = item.optString("sourceTreeUri").takeIf { it.isNotBlank() }?.let(Uri::parse),
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
            array.put(toJson(merge(item, previous)))
        }
        preferences.edit().putString(KEY_ITEMS, array.toString()).apply()
    }

    fun mergeScan(sourceTreeUri: Uri, items: List<LibraryRom>) {
        val source = sourceTreeUri.toString()
        val existing = list()
        val existingByUri = existing.associateBy { it.uri }
        val scanned = items.distinctBy { it.uri }.map { item ->
            item.copy(sourceTreeUri = sourceTreeUri)
        }
        val scannedUris = scanned.map { it.uri }.toSet()
        val preserved = existing.filter { item ->
            item.uri !in scannedUris && !belongsToSource(item, source)
        }
        val array = JSONArray()
        (preserved + scanned.map { item -> merge(item, existingByUri[item.uri]) })
            .sortedWith(librarySort)
            .forEach { item -> array.put(toJson(item)) }
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

    fun setCoverPath(uri: Uri, coverPath: String) {
        val array = JSONArray()
        list().forEach { item ->
            val updated = if (item.uri == uri) item.copy(coverPath = coverPath) else item
            array.put(toJson(updated))
        }
        preferences.edit().putString(KEY_ITEMS, array.toString()).apply()
    }

    fun addPlayTime(uri: Uri, seconds: Long) {
        if (seconds <= 0L) {
            return
        }
        val array = JSONArray()
        list().forEach { item ->
            val updated = if (item.uri == uri) {
                item.copy(playTimeSeconds = item.playTimeSeconds + seconds)
            } else {
                item
            }
            array.put(toJson(updated))
        }
        preferences.edit().putString(KEY_ITEMS, array.toString()).apply()
    }

    private fun toJson(item: LibraryRom): JSONObject {
        return JSONObject()
            .put("uri", item.uri.toString())
            .put("displayName", item.displayName)
            .put("title", item.title)
            .put("platform", item.platform)
            .put("system", item.system)
            .put("gameCode", item.gameCode)
            .put("maker", item.maker)
            .put("version", item.version)
            .put("crc32", item.crc32)
            .put("sha1", item.sha1)
            .put("fileSize", item.fileSize)
            .put("lastPlayedAt", item.lastPlayedAt)
            .put("playTimeSeconds", item.playTimeSeconds)
            .put("favorite", item.favorite)
            .put("coverPath", item.coverPath)
            .put("sourceTreeUri", item.sourceTreeUri?.toString().orEmpty())
    }

    private fun merge(item: LibraryRom, previous: LibraryRom?): LibraryRom {
        return item.copy(
            title = item.title.ifBlank { previous?.title.orEmpty() },
            platform = item.platform.ifBlank { previous?.platform.orEmpty() },
            system = item.system.ifBlank { previous?.system.orEmpty() },
            gameCode = item.gameCode.ifBlank { previous?.gameCode.orEmpty() },
            maker = item.maker.ifBlank { previous?.maker.orEmpty() },
            version = if (item.version >= 0) item.version else previous?.version ?: -1,
            crc32 = item.crc32.ifBlank { previous?.crc32.orEmpty() },
            sha1 = item.sha1.ifBlank { previous?.sha1.orEmpty() },
            fileSize = if (item.fileSize > 0L) item.fileSize else previous?.fileSize ?: 0L,
            lastPlayedAt = previous?.lastPlayedAt ?: item.lastPlayedAt,
            playTimeSeconds = previous?.playTimeSeconds ?: item.playTimeSeconds,
            favorite = previous?.favorite ?: item.favorite,
            coverPath = item.coverPath.ifBlank { previous?.coverPath.orEmpty() },
            sourceTreeUri = item.sourceTreeUri ?: previous?.sourceTreeUri,
        )
    }

    private fun belongsToSource(item: LibraryRom, sourceTreeUri: String): Boolean {
        val storedSource = item.sourceTreeUri?.toString()
        if (storedSource != null) {
            return storedSource == sourceTreeUri
        }
        return item.uri.toString().startsWith("$sourceTreeUri/document/")
    }

    private companion object {
        const val KEY_ITEMS = "items"
        val librarySort = compareByDescending<LibraryRom> { it.favorite }.thenBy { it.displayName.lowercase() }
    }
}
