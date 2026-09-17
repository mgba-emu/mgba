/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */


package org.mgba_emu.mgba.renderer.gl

import org.mgba_emu.mgba.core.Core
import org.mgba_emu.mgba.input.InputState
import org.mgba_emu.mgba.utils.GlobalConfig
import java.util.concurrent.atomic.AtomicBoolean

class EmulationThread(
    private val inputState: InputState,
    private val frameBuffer: FrameBuffer,
    private val onFrameReady: () -> Unit,
    private val onFpsUpdated: (Double) -> Unit
) : Thread("mgba-emulation") {

    private val running = AtomicBoolean(false)

    @Volatile
    var paused: Boolean = false

    private val frameLimit = when (GlobalConfig.frameLimit) {
        0 -> NATIVE_FPS
        1 -> 60.0
        2 -> 120.0
        else -> NATIVE_FPS
    }
    private val frameIntervalNs = (NANOS_PER_SECOND / frameLimit).toLong()
    private val maxLagNs = (NANOS_PER_SECOND / frameLimit * 5).toLong()

    private var framesThisSecond = 0
    private var lastFpsTimestampNs = 0L

    override fun run() {
        running.set(true)
        var nextFrameDeadlineNs = System.nanoTime()
        lastFpsTimestampNs = System.nanoTime()

        while (running.get()) {
            if (paused) {
                sleepQuietly()
                nextFrameDeadlineNs = System.nanoTime()
                lastFpsTimestampNs = System.nanoTime()
                continue
            }

            stepOneFrame()
            calculateFps()

            nextFrameDeadlineNs += frameIntervalNs
            val now = System.nanoTime()
            val remainingNs = nextFrameDeadlineNs - now

            if (GlobalConfig.fastForward) {
                // skip sleeping to run as fast as possible
                nextFrameDeadlineNs = now
            } else {
                if (remainingNs > 0) {
                    sleepNanos(remainingNs)
                } else if (remainingNs < -maxLagNs) {
                    nextFrameDeadlineNs = now
                }
            }
        }
    }

    private fun calculateFps() {
        framesThisSecond++
        val now = System.nanoTime()
        val elapsedNs = now - lastFpsTimestampNs

        if (elapsedNs >= 1_000_000_000L) {
            val exactFps = (framesThisSecond * 1_000_000_000.0) / elapsedNs

            onFpsUpdated(exactFps)
            framesThisSecond = 0
            lastFpsTimestampNs = now
        }
    }

    private fun stepOneFrame() {
        Core.setKeys(inputState.current())
        Core.runFrame()

        val width = Core.width
        val height = Core.height
        if (width > 0 && height > 0) {
            val pixels = Core.getVideoBuffer()
            frameBuffer.publish(pixels, width, height)
            onFrameReady()
        }
    }

    fun requestStop() {
        running.set(false)
        interrupt()
    }

    private fun sleepNanos(nanos: Long) {
        try {
            val millis = nanos / 1_000_000
            val remainderNanos = (nanos % 1_000_000).toInt()
            sleep(millis, remainderNanos)
        } catch (_: InterruptedException) {
            // expected on requestStop()
        }
    }

    private fun sleepQuietly(millis: Long = 50) {
        try {
            sleep(millis)
        } catch (_: InterruptedException) {
            // expected on requestStop()
        }
    }

    companion object {
        private const val NANOS_PER_SECOND = 1_000_000_000.0
        private const val NATIVE_FPS = 16777216.0 / 280896.0
    }
}