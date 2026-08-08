package org.mgba_emu.mgba.input

// Bit values matching GbaKey in mGBA's GBAKey enum
object GbaKey {
    const val A: Int = 1 shl 0
    const val B: Int = 1 shl 1
    const val SELECT: Int = 1 shl 2
    const val START: Int = 1 shl 3
    const val RIGHT: Int = 1 shl 4
    const val LEFT: Int = 1 shl 5
    const val UP: Int = 1 shl 6
    const val DOWN: Int = 1 shl 7
    const val R: Int = 1 shl 8
    const val L: Int = 1 shl 9
}
