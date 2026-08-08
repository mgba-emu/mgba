
package org.mgba_emu.mgba.utils

import org.mgba_emu.mgba.mGBAApplication
import org.mgba_emu.mgba.core.Platform
import java.io.File
import java.io.IOException

object BiosStore {
    private val biosDir: File by lazy {
        File(mGBAApplication.context.getExternalFilesDir(null), "bios").apply { mkdirs() }
    }

    private fun fileFor(platform: Platform): File {
        val name = when (platform) {
            Platform.GB -> "gb"
            Platform.GBC -> "gbc"
            Platform.SGB -> "sgb"
            Platform.GBA -> "gba"
            else -> ByteArray(0)
        }
        return File(biosDir, "$name.bin")
    }

    fun has(platform: Platform): Boolean = fileFor(platform).exists()

    fun load(platform: Platform): ByteArray {
        val file = fileFor(platform)
        if (!file.exists()) return ByteArray(0)
        return try {
            file.readBytes()
        } catch (_: IOException) {
            ByteArray(0)
        }
    }

    fun import(platform: Platform, biosBytes: ByteArray): Boolean {
        if (biosBytes.isEmpty()) return false
        val target = fileFor(platform)
        val temp = File(biosDir, "${target.name}.tmp")
        return try {
            temp.writeBytes(biosBytes)
            temp.renameTo(target)
        } catch (_: IOException) {
            false
        }
    }

    fun clear(platform: Platform) {
        fileFor(platform).delete()
    }
}