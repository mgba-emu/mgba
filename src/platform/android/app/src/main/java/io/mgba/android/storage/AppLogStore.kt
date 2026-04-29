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

    fun recent(context: Context): String {
        synchronized(lock) {
            val file = logFile(context) ?: return ""
            if (!file.isFile) {
                return ""
            }
            return file.readText()
        }
    }

    private fun logFile(context: Context): File? {
        val directory = File(context.applicationContext.filesDir, "logs")
        if (!directory.exists() && !directory.mkdirs()) {
            return null
        }
        return File(directory, "app.log")
    }

    private fun trimIfNeeded(file: File) {
        if (!file.isFile || file.length() <= MAX_BYTES) {
            return
        }
        val bytes = file.readBytes()
        val keep = bytes.takeLast(TRIM_BYTES)
        file.writeBytes(keep.toByteArray())
    }

    private const val MAX_BYTES = 128 * 1024
    private const val TRIM_BYTES = 64 * 1024
}
