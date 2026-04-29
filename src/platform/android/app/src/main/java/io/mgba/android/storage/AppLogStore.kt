package io.mgba.android.storage

import android.content.Context
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

object AppLogStore {
    private val lock = Any()
    private val timestampFormat = SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS", Locale.US)

    fun append(context: Context, message: String, throwable: Throwable? = null) {
        synchronized(lock) {
            val file = logFile(context) ?: return
            trimIfNeeded(file)
            file.appendText(buildString {
                append(timestampFormat.format(Date()))
                append(" ")
                appendLine(message)
                if (throwable != null) {
                    appendLine(throwable.stackTraceToString())
                }
            })
        }
    }

    fun markCrash(context: Context, message: String) {
        synchronized(lock) {
            val file = crashMarkerFile(context) ?: return
            replaceFileAtomically(file) { temp ->
                temp.writeText(buildString {
                    append(timestampFormat.format(Date()))
                    append(" ")
                    appendLine(message)
                })
            }
        }
    }

    fun consumeCrashMarker(context: Context): String? {
        synchronized(lock) {
            val file = crashMarkerFile(context) ?: return null
            if (!file.isFile) {
                return null
            }
            val message = file.readText().trim().ifEmpty { "The previous session crashed." }
            file.delete()
            return message
        }
    }

    fun hasConsumedProcessExit(context: Context, timestampMs: Long): Boolean {
        synchronized(lock) {
            val file = consumedExitFile(context) ?: return false
            if (!file.isFile) {
                return false
            }
            return file.readText().trim().toLongOrNull() == timestampMs
        }
    }

    fun markProcessExitConsumed(context: Context, timestampMs: Long) {
        synchronized(lock) {
            val file = consumedExitFile(context) ?: return
            replaceFileAtomically(file) { temp ->
                temp.writeText(timestampMs.toString())
            }
        }
    }

    fun recent(context: Context): String {
        synchronized(lock) {
            val file = logFile(context) ?: return ""
            if (!file.isFile) {
                return ""
            }
            return file.readText()
        }
    }

    fun clear(context: Context): Boolean {
        synchronized(lock) {
            val directory = logDirectory(context) ?: return false
            directory.listFiles()?.forEach { file -> file.delete() }
            return true
        }
    }

    private fun logFile(context: Context): File? {
        return File(logDirectory(context) ?: return null, "app.log")
    }

    private fun crashMarkerFile(context: Context): File? {
        return File(logDirectory(context) ?: return null, "crash.marker")
    }

    private fun consumedExitFile(context: Context): File? {
        return File(logDirectory(context) ?: return null, "last-consumed-exit.txt")
    }

    private fun logDirectory(context: Context): File? {
        val directory = File(context.applicationContext.filesDir, "logs")
        if (!directory.exists() && !directory.mkdirs()) {
            return null
        }
        return directory
    }

    private fun trimIfNeeded(file: File) {
        if (!file.isFile || file.length() <= MAX_BYTES) {
            return
        }
        val bytes = file.readBytes()
        val keep = bytes.takeLast(TRIM_BYTES)
        replaceFileAtomically(file) { temp ->
            temp.writeBytes(keep.toByteArray())
        }
    }

    private const val MAX_BYTES = 128 * 1024
    private const val TRIM_BYTES = 64 * 1024
}
