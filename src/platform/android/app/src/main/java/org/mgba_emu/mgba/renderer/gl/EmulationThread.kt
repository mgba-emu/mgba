
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
) : Thread("gbdroid-emulation") {

    private val running = AtomicBoolean(false)

    @Volatile
    var paused: Boolean = false

    private var framesThisSecond = 0
    private var lastFpsTimestampNs = 0L

    override fun run() {
        running.set(true)
        val frameIntervalNs = NANOS_PER_SECOND / GBA_FPS
        var nextFrameDeadlineNs = System.nanoTime()
        lastFpsTimestampNs = System.nanoTime()

        while (running.get()) {
            if (paused) {
                sleepQuietly(50)
                nextFrameDeadlineNs = System.nanoTime()
                lastFpsTimestampNs = System.nanoTime()
                continue
            }

            stepOneFrame()
            calculateFps()

            nextFrameDeadlineNs += frameIntervalNs.toLong()
            val now = System.nanoTime()
            val remainingNs = nextFrameDeadlineNs - now

            if (GlobalConfig.fastForward) {
                // skip sleeping to run as fast as possible
                nextFrameDeadlineNs = now
            } else {
                if (remainingNs > 0) {
                    sleepNanos(remainingNs)
                } else if (remainingNs < -MAX_LAG_NS) {
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

    private fun sleepQuietly(millis: Long) {
        try {
            sleep(millis)
        } catch (_: InterruptedException) {
            // expected on requestStop()
        }
    }

    companion object {
        private const val NANOS_PER_SECOND = 1_000_000_000.0
        const val GBA_FPS = 16777216.0 / 280896.0
        private val MAX_LAG_NS = (NANOS_PER_SECOND / GBA_FPS * 5).toLong()
    }
}