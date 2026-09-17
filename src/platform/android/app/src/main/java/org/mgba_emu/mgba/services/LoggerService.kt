/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */


package org.mgba_emu.mgba.services

import android.app.Service
import android.content.Intent
import android.content.pm.PackageInfo
import android.os.Build
import android.os.IBinder
import android.util.Log
import java.io.File
import java.io.FileOutputStream
import java.io.InputStream
import java.io.OutputStream
import java.util.concurrent.atomic.AtomicBoolean

class LoggerService : Service() {

    companion object {
        private const val LOG_TAG = "LoggerService"
        private const val MAX_LOG_SIZE = 1024L * 1024L * 4L
    }

    private var errorThread: StreamPipeThread? = null
    private var outputThread: StreamPipeThread? = null
    private var logcat: Process? = null

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        try {
            Runtime.getRuntime().exec(arrayOf("logcat", "-c")).waitFor()
            val proc = Runtime.getRuntime().exec(arrayOf("logcat"))
            logcat = proc

            val baseMediaDir = externalMediaDirs.firstOrNull() ?: filesDir
            val logsDir = File(baseMediaDir, "logs").apply { if (!exists()) mkdirs() }

            val lastFile = File(logsDir, "last.txt")
            if (lastFile.exists()) {
                lastFile.delete()
            }

            val currentFile = File(logsDir, "current.txt")
            if (currentFile.exists()) {
                currentFile.renameTo(lastFile)
            }

            val stream = FileOutputStream(currentFile)

            errorThread = StreamPipeThread(proc.errorStream, stream, MAX_LOG_SIZE).apply { start() }
            outputThread = StreamPipeThread(proc.inputStream, stream, MAX_LOG_SIZE).apply { start() }

            Log.i(LOG_TAG, "Started logger service")
            logDeviceInfo()
        } catch (e: Exception) {
            stopSelf()
            Log.e(LOG_TAG, "Failed to start logger service", e)
        }
    }

    private fun logDeviceInfo() {
        Log.i(LOG_TAG, "----------------------")
        Log.i(LOG_TAG, "Android SDK: ${Build.VERSION.SDK_INT}")
        Log.i(LOG_TAG, "Device: ${Build.DEVICE}")
        Log.i(LOG_TAG, "Model: ${Build.MANUFACTURER} ${Build.MODEL}")
        Log.i(LOG_TAG, "ABIs: ${Build.SUPPORTED_ABIS.contentToString()}")
        try {
            val info: PackageInfo = packageManager.getPackageInfo(packageName, 0)
            Log.i(LOG_TAG, "")
            Log.i(LOG_TAG, "Package: ${info.packageName}")
            @Suppress("DEPRECATION")
            Log.i(LOG_TAG, "Install location: ${info.installLocation}")
            @Suppress("DEPRECATION")
            Log.i(LOG_TAG, "App version: ${info.versionName} (${info.versionCode})")
        } catch (e: Exception) {
            Log.e(LOG_TAG, "Error obtaining package info: $e")
        }
        Log.i(LOG_TAG, "----------------------")
    }

    override fun onTaskRemoved(rootIntent: Intent?) {
        stopSelf()
        try {
            Thread.sleep(1000)
        } catch (_: Exception) {}
        super.onTaskRemoved(rootIntent)
    }

    override fun onDestroy() {
        Log.i(LOG_TAG, "Logger service terminating")
        errorThread?.close()
        outputThread?.close()
        try {
            logcat?.destroy()
        } catch (_: Throwable) {}
        super.onDestroy()
    }

    private class StreamPipeThread(
        private val inputStream: InputStream,
        private val outputStream: OutputStream,
        private val maxBytes: Long
    ) : Thread() {

        private val isRunning = AtomicBoolean(true)

        override fun run() {
            val buffer = ByteArray(4096)
            var totalBytesWritten = 0L

            try {
                var bytesRead: Int
                while (isRunning.get() && totalBytesWritten < maxBytes) {
                    bytesRead = inputStream.read(buffer)
                    if (bytesRead == -1) break

                    synchronized(outputStream) {
                        outputStream.write(buffer, 0, bytesRead)
                        outputStream.flush()
                    }
                    totalBytesWritten += bytesRead
                }
            } catch (_: Exception) {}
        }

        fun close() {
            isRunning.set(false)
            interrupt()
        }
    }
}