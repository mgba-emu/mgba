package io.mgba.android.storage

import java.nio.file.Files
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class StorageFilesTest {
    @Test
    fun replacesExistingFileAtomically() {
        val directory = Files.createTempDirectory("mgba-storage-files").toFile()
        val target = directory.resolve("game.cheats")
        target.writeText("old")

        val replaced = replaceFileAtomically(target) { temp ->
            temp.writeText("new")
        }

        assertTrue(replaced)
        assertEquals("new", target.readText())
        assertFalse(directory.listFiles().orEmpty().any { it.name.endsWith(".tmp") || it.name.endsWith(".bak") })
    }

    @Test
    fun keepsOriginalFileWhenWriteFails() {
        val directory = Files.createTempDirectory("mgba-storage-files").toFile()
        val target = directory.resolve("gba.bios")
        target.writeText("original")

        val replaced = replaceFileAtomically(target) { temp ->
            temp.writeText("partial")
            error("copy failed")
        }

        assertFalse(replaced)
        assertEquals("original", target.readText())
        assertFalse(directory.listFiles().orEmpty().any { it.name.endsWith(".tmp") || it.name.endsWith(".bak") })
    }
}
