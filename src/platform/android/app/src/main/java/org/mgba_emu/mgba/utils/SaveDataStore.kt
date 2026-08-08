
package org.mgba_emu.mgba.utils

import org.mgba_emu.mgba.mGBAApplication
import java.io.File
import java.io.IOException

object SaveDataStore {
    private val saveDir: File by lazy {
        File(mGBAApplication.context.getExternalFilesDir(null), "saves").apply { mkdirs() }
    }

    private fun fileFor(gameCode: String): File {
        val safeCode = gameCode.filter { it.isLetterOrDigit() }.ifEmpty { "UNKNOWN" }
        return File(saveDir, "$safeCode.sav")
    }

    fun load(gameCode: String): ByteArray {
        val file = fileFor(gameCode)
        if (!file.exists()) return ByteArray(0)
        return try {
            file.readBytes()
        } catch (_: IOException) {
            ByteArray(0)
        }
    }

    fun save(gameCode: String, saveBytes: ByteArray): Boolean {
        if (saveBytes.isEmpty()) return true
        val target = fileFor(gameCode)
        val temp = File(saveDir, "${target.name}.tmp")
        return try {
            temp.writeBytes(saveBytes)
            temp.renameTo(target)
        } catch (_: IOException) {
            false
        }
    }
}