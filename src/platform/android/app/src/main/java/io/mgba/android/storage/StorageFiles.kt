package io.mgba.android.storage

import java.io.File
import java.util.concurrent.atomic.AtomicLong

private val storageFileToken = AtomicLong()

fun replaceFileAtomically(target: File, writeTemp: (File) -> Unit): Boolean {
    val directory = target.parentFile ?: return false
    if (!directory.exists() && !directory.mkdirs()) {
        return false
    }
    val token = "${System.nanoTime()}-${storageFileToken.incrementAndGet()}"
    val temp = File(directory, "${target.name}.$token.tmp")
    val backup = File(directory, "${target.name}.$token.bak")
    var originalMoved = false
    return try {
        temp.delete()
        backup.delete()
        writeTemp(temp)
        if (!temp.isFile) {
            false
        } else {
            if (target.exists()) {
                if (!target.renameTo(backup)) {
                    return false
                }
                originalMoved = true
            }
            if (!temp.renameTo(target)) {
                if (originalMoved && !target.exists()) {
                    backup.renameTo(target)
                }
                false
            } else {
                originalMoved = false
                backup.delete()
                true
            }
        }
    } catch (_: Throwable) {
        if (originalMoved && !target.exists()) {
            backup.renameTo(target)
        }
        false
    } finally {
        temp.delete()
        if (!originalMoved) {
            backup.delete()
        }
    }
}
