package io.mgba.android.storage

import android.content.ContentProvider
import android.content.ContentValues
import android.content.Context
import android.database.Cursor
import android.net.Uri
import android.os.ParcelFileDescriptor
import java.io.File
import java.io.FileNotFoundException

class ScreenshotShareProvider : ContentProvider() {
    override fun onCreate(): Boolean = true

    override fun getType(uri: Uri): String = "image/bmp"

    override fun openFile(uri: Uri, mode: String): ParcelFileDescriptor {
        if (mode != "r") {
            throw FileNotFoundException("Unsupported mode")
        }
        val context = context ?: throw FileNotFoundException("Provider is unavailable")
        val file = fileForUri(context, uri)
        return ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY)
    }

    override fun query(
        uri: Uri,
        projection: Array<out String>?,
        selection: String?,
        selectionArgs: Array<out String>?,
        sortOrder: String?,
    ): Cursor? = null

    override fun insert(uri: Uri, values: ContentValues?): Uri? = null

    override fun delete(uri: Uri, selection: String?, selectionArgs: Array<out String>?): Int = 0

    override fun update(uri: Uri, values: ContentValues?, selection: String?, selectionArgs: Array<out String>?): Int = 0

    private fun fileForUri(context: Context, uri: Uri): File {
        if (uri.authority != authority(context)) {
            throw FileNotFoundException("Unexpected authority")
        }
        val name = uri.lastPathSegment ?: throw FileNotFoundException("Missing file name")
        val base = File(context.filesDir, "screenshots").canonicalFile
        val file = File(base, name).canonicalFile
        if (!file.path.startsWith(base.path + File.separator) || !file.isFile) {
            throw FileNotFoundException("Screenshot is unavailable")
        }
        return file
    }

    companion object {
        fun uriFor(context: Context, path: String): Uri {
            return Uri.Builder()
                .scheme("content")
                .authority(authority(context))
                .appendPath(File(path).name)
                .build()
        }

        private fun authority(context: Context): String {
            return "${context.packageName}.screenshots"
        }
    }
}
