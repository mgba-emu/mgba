package io.mgba.android.storage

import android.content.Context
import android.net.Uri
import java.io.File
import java.security.MessageDigest

data class CheatEntry(
    val name: String,
    val enabled: Boolean,
    val lines: List<String>,
    val directives: List<String> = emptyList(),
)

class CheatStore(context: Context) {
    private val appContext = context.applicationContext
    private val preferences = appContext.getSharedPreferences("cheats", Context.MODE_PRIVATE)

    fun importForGame(gameId: String?, uri: Uri, displayName: String): Boolean {
        val id = cheatId(gameId) ?: return false
        val directory = cheatDirectory() ?: return false
        val target = File(directory, "$id.cheats")
        val imported = replaceFileAtomically(target) { temp ->
            val input = appContext.contentResolver.openInputStream(uri) ?: error("Cheat input unavailable")
            input.use {
                temp.outputStream().use { output ->
                    it.copyTo(output)
                }
            }
        }
        if (imported) {
            preferences.edit()
                .putString(gameDisplayNameKey(id), displayName)
                .putString(gameFileNameKey(id), target.name)
                .apply()
        }
        return imported
    }

    fun importForGameFile(gameId: String?, file: File, displayName: String): Boolean {
        val id = cheatId(gameId) ?: return false
        val directory = cheatDirectory() ?: return false
        val target = File(directory, "$id.cheats")
        val imported = replaceFileAtomically(target) { temp ->
            file.copyTo(temp, overwrite = true)
        }
        if (imported) {
            preferences.edit()
                .putString(gameDisplayNameKey(id), displayName)
                .putString(gameFileNameKey(id), target.name)
                .apply()
        }
        return imported
    }

    fun fileForGame(gameId: String?): File? {
        val id = cheatId(gameId) ?: return null
        val fileName = preferences.getString(gameFileNameKey(id), null) ?: return null
        val file = File(File(appContext.filesDir, CHEAT_DIRECTORY), fileName)
        return file.takeIf { it.isFile }
    }

    fun displayNameForGame(gameId: String?): String? {
        val id = cheatId(gameId) ?: return null
        return preferences.getString(gameDisplayNameKey(id), null)
    }

    fun entriesForGame(gameId: String?): List<CheatEntry> {
        val file = fileForGame(gameId) ?: return emptyList()
        return runCatching { parseEntries(file.readLines()) }.getOrDefault(emptyList())
    }

    fun setEnabled(gameId: String?, index: Int, enabled: Boolean): Boolean {
        val file = fileForGame(gameId) ?: return false
        val entries = entriesForGame(gameId).toMutableList()
        if (index !in entries.indices) {
            return false
        }
        entries[index] = entries[index].copy(enabled = enabled)
        return runCatching {
            file.writeText(serializeEntries(entries))
            true
        }.getOrDefault(false)
    }

    fun addManual(gameId: String?, name: String, codeText: String): Boolean {
        val id = cheatId(gameId) ?: return false
        val directory = cheatDirectory() ?: return false
        val target = File(directory, "$id.cheats")
        val lines = codeLines(codeText).takeIf { it.isNotEmpty() } ?: return false
        val entries = entriesForGame(gameId).toMutableList()
        entries += CheatEntry(
            name = name.ifBlank { "Manual cheat ${entries.size + 1}" },
            enabled = true,
            lines = lines,
        )
        val saved = replaceFileAtomically(target) { temp ->
            temp.writeText(serializeEntries(entries))
        }
        if (saved) {
            preferences.edit()
                .putString(gameDisplayNameKey(id), "Manual cheats")
                .putString(gameFileNameKey(id), target.name)
                .apply()
        }
        return saved
    }

    fun updateEntry(gameId: String?, index: Int, name: String, codeText: String): Boolean {
        val file = fileForGame(gameId) ?: return false
        val lines = codeLines(codeText).takeIf { it.isNotEmpty() } ?: return false
        val entries = entriesForGame(gameId).toMutableList()
        if (index !in entries.indices) {
            return false
        }
        val current = entries[index]
        entries[index] = current.copy(
            name = name.ifBlank { current.name },
            lines = lines,
        )
        return writeEntries(file, entries)
    }

    fun removeEntry(gameId: String?, index: Int): Boolean {
        val file = fileForGame(gameId) ?: return false
        val entries = entriesForGame(gameId).toMutableList()
        if (index !in entries.indices) {
            return false
        }
        entries.removeAt(index)
        return writeEntries(file, entries)
    }

    fun clearForGame(gameId: String?): Boolean {
        val id = cheatId(gameId) ?: return false
        val fileName = preferences.getString(gameFileNameKey(id), null)
        val deleted = runCatching {
            if (fileName == null) {
                true
            } else {
                val file = File(File(appContext.filesDir, CHEAT_DIRECTORY), fileName)
                !file.exists() || file.delete()
            }
        }.getOrDefault(false)
        if (deleted) {
            preferences.edit()
                .remove(gameDisplayNameKey(id))
                .remove(gameFileNameKey(id))
                .apply()
        }
        return deleted
    }

    private fun cheatDirectory(): File? {
        val directory = File(appContext.filesDir, CHEAT_DIRECTORY)
        return if (directory.exists() || directory.mkdirs()) directory else null
    }

    private fun codeLines(codeText: String): List<String> {
        return codeText
            .lineSequence()
            .map { it.trim() }
            .filter { it.isNotBlank() }
            .toList()
    }

    private fun writeEntries(file: File, entries: List<CheatEntry>): Boolean {
        return replaceFileAtomically(file) { temp ->
            temp.writeText(serializeEntries(entries))
        }
    }

    private fun parseEntries(lines: List<String>): List<CheatEntry> {
        val entries = mutableListOf<CheatEntry>()
        var name = "Cheats"
        var enabled = true
        var codes = mutableListOf<String>()
        var directives = mutableListOf<String>()
        var pendingDisabled = false
        var pendingDirectives = mutableListOf<String>()

        fun flush() {
            if (codes.isNotEmpty()) {
                entries += CheatEntry(name, enabled, codes.toList(), directives.toList())
            }
            codes = mutableListOf()
            directives = mutableListOf()
        }

        lines.forEach { raw ->
            val line = raw.trim()
            when {
                line.startsWith("#") -> {
                    flush()
                    name = line.removePrefix("#").trim().ifBlank { "Cheats ${entries.size + 1}" }
                    enabled = !pendingDisabled
                    directives = pendingDirectives
                    pendingDisabled = false
                    pendingDirectives = mutableListOf()
                }
                line.startsWith("!") -> {
                    val directive = line.removePrefix("!").trim()
                    if (directive.equals("disabled", ignoreCase = true)) {
                        pendingDisabled = true
                    } else if (directive.equals("reset", ignoreCase = true)) {
                        pendingDirectives.clear()
                    } else if (directive.isNotBlank()) {
                        pendingDirectives += directive
                    }
                }
                line.isNotBlank() -> {
                    if (codes.isEmpty() && entries.isEmpty() && name == "Cheats") {
                        enabled = !pendingDisabled
                        directives = pendingDirectives
                        pendingDisabled = false
                        pendingDirectives = mutableListOf()
                    }
                    codes += line
                }
            }
        }
        flush()
        return entries
    }

    private fun serializeEntries(entries: List<CheatEntry>): String {
        return buildString {
            entries.forEach { entry ->
                if (!entry.enabled) {
                    appendLine("!disabled")
                }
                entry.directives.forEach { directive ->
                    appendLine("!$directive")
                }
                appendLine("# ${entry.name}")
                entry.lines.forEach { line ->
                    appendLine(line)
                }
            }
        }
    }

    private fun cheatId(gameId: String?): String? {
        val value = gameId?.takeIf { it.isNotBlank() } ?: return null
        val digest = MessageDigest.getInstance("SHA-1").digest(value.toByteArray(Charsets.UTF_8))
        return digest.joinToString("") { "%02x".format(it.toInt() and 0xFF) }
    }

    private fun gameDisplayNameKey(id: String): String {
        return "$KEY_GAME_DISPLAY_NAME_PREFIX$id"
    }

    private fun gameFileNameKey(id: String): String {
        return "$KEY_GAME_FILE_NAME_PREFIX$id"
    }

    companion object {
        private const val KEY_GAME_DISPLAY_NAME_PREFIX = "gameDisplayName:"
        private const val KEY_GAME_FILE_NAME_PREFIX = "gameFileName:"
        private const val CHEAT_DIRECTORY = "cheats"
    }
}
