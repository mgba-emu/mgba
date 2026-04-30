package io.mgba.android.settings

import org.junit.Assert.assertEquals
import org.junit.Test

class FastForwardModesTest {
    @Test
    fun defaultMultiplierUsesBoundedSpeed() {
        assertEquals(200, FastForwardModes.MultiplierDefault)
        assertEquals("2x", FastForwardModes.labelForMultiplier(FastForwardModes.MultiplierDefault))
    }

    @Test
    fun multiplierCycleIncludesRequestedSpeeds() {
        assertEquals(125, FastForwardModes.nextMultiplier(100))
        assertEquals(150, FastForwardModes.nextMultiplier(125))
        assertEquals(200, FastForwardModes.nextMultiplier(150))
        assertEquals(300, FastForwardModes.nextMultiplier(200))
        assertEquals(400, FastForwardModes.nextMultiplier(300))
        assertEquals(800, FastForwardModes.nextMultiplier(400))
        assertEquals(100, FastForwardModes.nextMultiplier(800))
    }

    @Test
    fun coercesLegacyValues() {
        assertEquals(200, FastForwardModes.coerceMultiplier(0))
        assertEquals(100, FastForwardModes.coerceMultiplier(1))
        assertEquals(200, FastForwardModes.coerceMultiplier(2))
        assertEquals(300, FastForwardModes.coerceMultiplier(3))
        assertEquals(400, FastForwardModes.coerceMultiplier(4))
    }
}
