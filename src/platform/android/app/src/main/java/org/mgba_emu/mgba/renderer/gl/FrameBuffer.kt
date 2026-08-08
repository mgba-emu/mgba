
package org.mgba_emu.mgba.renderer.gl

import java.util.concurrent.atomic.AtomicReference

class FrameBuffer(initialPixelCount: Int) {

    data class Frame(val pixels: IntArray, val width: Int, val height: Int)

    @Volatile private var slots = Array(3) { IntArray(initialPixelCount) }
    @Volatile private var slotPixelCount = initialPixelCount
    private var backIndex = 0

    private val readyIndex = AtomicReference(-1)
    @Volatile private var readyWidth = 0
    @Volatile private var readyHeight = 0

    private val consumerHoldingIndex = AtomicReference(-1)

    fun publish(source: IntArray, width: Int, height: Int) {
        val requiredCount = width * height
        if (requiredCount != slotPixelCount) {
            slots = Array(3) { IntArray(requiredCount) }
            slotPixelCount = requiredCount
            backIndex = 0
            consumerHoldingIndex.set(-1)
        }

        val target = slots[backIndex]
        val count = minOf(source.size, target.size)
        System.arraycopy(source, 0, target, 0, count)

        val publishedIndex = backIndex
        readyWidth = width
        readyHeight = height
        readyIndex.set(publishedIndex)

        val consumerIdx = consumerHoldingIndex.get()
        for (candidate in slots.indices) {
            if (candidate != publishedIndex && candidate != consumerIdx) {
                backIndex = candidate
                break
            }
        }
    }

    fun acquireLatest(): Frame? {
        val currentSlots = slots
        val idx = readyIndex.get()
        if (idx < 0 || idx >= currentSlots.size) return null
        consumerHoldingIndex.set(idx)
        return Frame(currentSlots[idx], readyWidth, readyHeight)
    }
}