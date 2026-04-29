package io.mgba.android.storage

import android.content.ContentValues
import android.content.Context
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.MediaStore
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

object LogExporter {
    fun recentLogFileName(): String {
        return "mgba-log-${timestamp()}.txt"
    }

    fun exportRecent(context: Context): Uri? {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            return null
        }

        val resolver = context.contentResolver
        val values = ContentValues().apply {
            put(MediaStore.MediaColumns.DISPLAY_NAME, recentLogFileName())
            put(MediaStore.MediaColumns.MIME_TYPE, "text/plain")
            put(MediaStore.MediaColumns.RELATIVE_PATH, "${Environment.DIRECTORY_DOCUMENTS}/mGBA")
            put(MediaStore.MediaColumns.IS_PENDING, 1)
        }
        val uri = resolver.insert(MediaStore.Files.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY), values) ?: return null
        return runCatching {
            val logText = collectLogs(context)
            resolver.openOutputStream(uri)?.bufferedWriter()?.use { writer ->
                writer.write(logText)
            } ?: error("Could not open media output")
            values.clear()
            values.put(MediaStore.MediaColumns.IS_PENDING, 0)
            resolver.update(uri, values, null, null)
            uri
        }.getOrElse {
            resolver.delete(uri, null, null)
            null
        }
    }

    fun writeRecent(context: Context, uri: Uri): Boolean {
        return runCatching {
            context.contentResolver.openOutputStream(uri)?.bufferedWriter()?.use { writer ->
                writer.write(collectLogs(context))
            } ?: return@runCatching false
            true
        }.getOrDefault(false)
    }

    private fun timestamp(): String {
        return SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(Date())
    }

    private fun collectLogs(context: Context): String {
        val process = Runtime.getRuntime().exec(arrayOf("logcat", "-d", "-t", "1000"))
        val output = process.inputStream.bufferedReader().use { it.readText() }
        val error = process.errorStream.bufferedReader().use { it.readText() }
        process.waitFor()
        val appLog = AppLogStore.recent(context)
        return buildString {
            appendLine("mGBA Android log export")
            appendLine("Generated: ${Date()}")
            appendLine()
            appendLine("App log ring buffer:")
            if (appLog.isBlank()) {
                appendLine("No app log entries were available.")
            } else {
                append(appLog)
            }
            appendLine()
            appendLine("Logcat:")
            if (output.isBlank() && error.isBlank()) {
                appendLine("No logcat output was available.")
            } else {
                append(output)
                if (error.isNotBlank()) {
                    appendLine()
                    appendLine("logcat stderr:")
                    append(error)
                }
            }
        }
    }
}
