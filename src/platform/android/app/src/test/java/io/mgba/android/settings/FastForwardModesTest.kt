package io.mgba.android.settings

import org.junit.Assert.assertEquals
import org.junit.Test

class FastForwardModesTest {
    @Test
    fun defaultMultiplierUsesBoundedSpeed() {
        assertEquals(2, FastForwardModes.MultiplierDefault)
        assertEquals("2x", FastForwardModes.labelForMultiplier(FastForwardModes.MultiplierDefault))
    }

    @Test
    fun multiplierCycleKeepsMaxAvailable() {
        assertEquals(3, FastForwardModes.nextMultiplier(2))
        assertEquals(4, FastForwardModes.nextMultiplier(3))
        assertEquals(FastForwardModes.MultiplierMax, FastForwardModes.nextMultiplier(4))
        assertEquals(2, FastForwardModes.nextMultiplier(FastForwardModes.MultiplierMax))
    }
}
