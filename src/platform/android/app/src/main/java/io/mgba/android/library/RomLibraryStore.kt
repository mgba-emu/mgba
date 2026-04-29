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

    fun sourceFolders(): List<Uri> {
        val json = preferences.getString(KEY_SOURCES, "[]") ?: "[]"
        val array = runCatching { JSONArray(json) }.getOrDefault(JSONArray())
        return buildList {
            for (index in 0 until array.length()) {
                array.optString(index).takeIf { it.isNotBlank() }?.let { source ->
                    add(Uri.parse(source))
                }
            }
        }
    }

    fun list(): List<LibraryRom> {
        val json = preferences.getString(KEY_ITEMS, "[]") ?: "[]"
        val array = runCatching { JSONArray(json) }.getOrDefault(JSONArray())
        return buildList {
            for (index in 0 until array.length()) {
                jsonToRom(array.optJSONObject(index))?.let(::add)
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
        preferences.edit()
            .putString(KEY_ITEMS, array.toString())
            .putString(KEY_SOURCES, sourceFoldersJson(sourceTreeUri))
            .apply()
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

    fun remove(uri: Uri): LibraryRom? {
        val items = list()
        val removed = items.firstOrNull { it.uri == uri }
        val array = JSONArray()
        items.filterNot { it.uri == uri }.forEach { item ->
            array.put(toJson(item))
        }
        preferences.edit().putString(KEY_ITEMS, array.toString()).apply()
        return removed
    }

    fun removeSourceFolder(sourceTreeUri: Uri): List<LibraryRom> {
        val source = sourceTreeUri.toString()
        val items = list()
        val remainingSources = sourceFolders().filterNot { it.toString() == source }
        val remainingItems = items.filterNot { belongsToSource(it, source) }
        val removedItems = items - remainingItems.toSet()
        val sourceArray = JSONArray()
        remainingSources.forEach { folder -> sourceArray.put(folder.toString()) }
        val itemArray = JSONArray()
        remainingItems.forEach { item -> itemArray.put(toJson(item)) }
        preferences.edit()
            .putString(KEY_SOURCES, sourceArray.toString())
            .putString(KEY_ITEMS, itemArray.toString())
            .apply()
        return removedItems
    }

    fun clearSourceFolders(): List<LibraryRom> {
        val sources = sourceFolders()
        val items = list()
        val remainingItems = items.filter { item ->
            sources.none { source -> belongsToSource(item, source.toString()) }
        }
        val removedItems = items - remainingItems.toSet()
        val itemArray = JSONArray()
        remainingItems.forEach { item -> itemArray.put(toJson(item)) }
        preferences.edit()
            .putString(KEY_SOURCES, JSONArray().toString())
            .putString(KEY_ITEMS, itemArray.toString())
            .apply()
        return removedItems
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

    fun exportJson(): JSONObject {
        val sources = JSONArray()
        sourceFolders().forEach { source -> sources.put(source.toString()) }
        val items = JSONArray()
        list().forEach { item -> items.put(toJson(item)) }
        return JSONObject()
            .put("version", 1)
            .put("sources", sources)
            .put("items", items)
    }

    fun importJson(json: JSONObject): Boolean {
        val items = json.optJSONArray("items") ?: return false
        val importedItems = buildList {
            for (index in 0 until items.length()) {
                jsonToRom(items.optJSONObject(index))?.let(::add)
            }
        }
        val itemArray = JSONArray()
        importedItems.distinctBy { it.uri }.sortedWith(librarySort).forEach { item ->
            itemArray.put(toJson(item))
        }
        val sourceArray = JSONArray()
        val sources = json.optJSONArray("sources") ?: JSONArray()
        for (index in 0 until sources.length()) {
            sources.optString(index).takeIf { it.isNotBlank() }?.let(sourceArray::put)
        }
        preferences.edit()
            .putString(KEY_ITEMS, itemArray.toString())
            .putString(KEY_SOURCES, sourceArray.toString())
            .apply()
        return true
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

    private fun jsonToRom(item: JSONObject?): LibraryRom? {
        if (item == null) {
            return null
        }
        val uri = item.optString("uri").takeIf { it.isNotBlank() } ?: return null
        return LibraryRom(
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
        )
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

    private fun sourceFoldersJson(sourceTreeUri: Uri): String {
        val sources = (sourceFolders() + sourceTreeUri).distinctBy { it.toString() }
        val array = JSONArray()
        sources.forEach { source -> array.put(source.toString()) }
        return array.toString()
    }

    private companion object {
        const val KEY_ITEMS = "items"
        const val KEY_SOURCES = "sources"
        val librarySort = compareByDescending<LibraryRom> { it.favorite }.thenBy { it.displayName.lowercase() }
    }
}
