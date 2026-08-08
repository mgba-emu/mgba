
package org.mgba_emu.mgba.input

import java.util.concurrent.atomic.AtomicInteger

class InputState {
    private val mask = AtomicInteger(0)

    fun press(key: Int) {
        mask.updateAndGet { it or key }
    }

    fun release(key: Int) {
        mask.updateAndGet { it and key.inv() }
    }

    fun current(): Int = mask.get()
}
