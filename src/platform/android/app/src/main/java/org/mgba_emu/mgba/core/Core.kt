/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */


package org.mgba_emu.mgba.core

import android.content.Context
import android.net.Uri
import android.util.Log
import org.mgba_emu.mgba.mGBAApplication
import org.mgba_emu.mgba.utils.GlobalConfig
import java.io.File

enum class Platform(val value: Int) {
    UNKNOWN(-1), GB(1), GBC(2), SGB(3), GBA(4);

    companion object {
        fun from(value: Int): Platform = when (value) {
            -1 -> UNKNOWN
            1 -> GB
            2 -> GBC
            3 -> SGB
            4 -> GBA
            else -> UNKNOWN
        }
    }
}

object Core {
    init {
        System.loadLibrary("mgba-android")
    }

    private var initialized = false

    var gameVersion = "v0"

    private var videoBuffer: IntArray = IntArray(0)

    var width: Int = 0
        private set
    var height: Int = 0
        private set

    fun init(): Boolean {
        if (initialized) return true
        initialized = nativeInit()
        if (initialized) {
            width = nativeGetWidth()
            height = nativeGetHeight()
            videoBuffer = IntArray(width * height)
        }
        return initialized
    }

    fun shutdown() {
        if (!initialized) return
        nativeShutdown()
        initialized = false
    }

    fun validateRom(uri: Uri): Boolean {
        check(initialized) { "Core.init() must succeed before loadRom()" }

        val parcel = mGBAApplication.context.contentResolver.openFileDescriptor(uri, "r")
        val romFd = parcel?.detachFd() ?: return false
        parcel.close()

        val ok = nativeValidateRom(romFd)
        return ok
    }

    fun loadRom(uri: Uri): Boolean {
        check(initialized) { "Core.init() must succeed before loadRom()" }

        val parcel = mGBAApplication.context.contentResolver.openFileDescriptor(uri, "r")
        val romFd = parcel?.detachFd() ?: return false
        parcel.close()

        val ok = nativeLoadRom(romFd, GlobalConfig.rtcEnable)
        if (ok) {
            nativeSetAudioMuted(GlobalConfig.mute)
            width = nativeGetWidth()
            height = nativeGetHeight()
            if (width > 0 && height > 0) {
                videoBuffer = IntArray(width * height)
            }
        }
        return ok
    }

    fun reset() = nativeReset()

    fun runFrame() = nativeRunFrame()

    // Returns the current frame's pixels as ARGB8888 ints and sized widthxheight
    fun getVideoBuffer(): IntArray {
        val buffer = videoBuffer
        nativeGetVideoBuffer(buffer)
        return buffer
    }

    fun setKeys(keyMask: Int) = nativeSetKeys(keyMask)

    fun gameTitle(): String = nativeGetGameTitle()
    fun gameCode(): String = nativeGetGameCode()

    fun getPlatform(): Platform {
        return Platform.from(nativeGetPlatform())
    }

    fun loadSaveData(saveBytes: ByteArray): Boolean = nativeLoadSaveData(saveBytes)
    fun exportSaveData(): ByteArray = nativeExportSaveData()

    fun loadBios(biosBytes: ByteArray): Boolean = nativeLoadBios(biosBytes)

    fun initNoIntroDB(context: Context) {
        val database = File(context.getExternalFilesDir(null), "database").apply { mkdirs() }
        val datFile = File(database, "nointro.dat")
        val dbFile = File(database, "nointro.db").apply { createNewFile() }

        if (dbFile.exists() && dbFile.readBytes().isNotEmpty()) {
            if (!nativeInitNoIntroDB("", dbFile.absolutePath)) {
                Log.e("Core", "nativeInitNoIntroDB Failed")
            }
            if (datFile.exists()) datFile.delete()
            return
        }

        if (!datFile.exists()) {
            context.assets.open("nointro.dat").use { input ->
                datFile.outputStream().use { outputStream ->
                    input.copyTo(outputStream)
                }
            }
        }

        nativeInitNoIntroDB(datFile.absolutePath, dbFile.absolutePath)
    }

    private external fun nativeInit(): Boolean
    private external fun nativeShutdown()
    private external fun nativeLoadRom(romFd: Int, rtcEnable: Boolean): Boolean
    private external fun nativeLoadBios(biosData: ByteArray): Boolean
    private external fun nativeValidateRom(romFd: Int): Boolean
    private external fun nativeReset()
    private external fun nativeRunFrame()
    private external fun nativeGetVideoBuffer(outPixels: IntArray)
    private external fun nativeGetWidth(): Int
    private external fun nativeGetHeight(): Int
    private external fun nativeSetKeys(keyMask: Int)
    private external fun nativeLoadSaveData(saveData: ByteArray): Boolean
    private external fun nativeExportSaveData(): ByteArray
    private external fun nativeGetGameTitle(): String
    private external fun nativeGetGameCode(): String
    private external fun nativeGetPlatform(): Int
    private external fun nativeSetAudioMuted(muted: Boolean)
    private external fun nativeInitNoIntroDB(datPath: String, dbPath: String): Boolean
}
