package io.mgba.android.bridge;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

public class NativeTypesTest {
    @Test
    public void parsesLoadResultJson() {
        NativeLoadResult result = NativeLoadResult.Companion.fromJson(
            "{"
                + "\"ok\":true,"
                + "\"message\":\"Loaded\","
                + "\"platform\":\"GBA\","
                + "\"system\":\"AGB\","
                + "\"title\":\"Test Game\","
                + "\"displayName\":\"test.gba\","
                + "\"crc32\":\"1234abcd\","
                + "\"gameCode\":\"ABCD\","
                + "\"maker\":\"01\","
                + "\"version\":2,"
                + "\"errorCode\":\"\""
                + "}"
        );

        assertTrue(result.getOk());
        assertEquals("Loaded", result.getMessage());
        assertEquals("GBA", result.getPlatform());
        assertEquals("AGB", result.getSystem());
        assertEquals("Test Game", result.getTitle());
        assertEquals("test.gba", result.getDisplayName());
        assertEquals("1234abcd", result.getCrc32());
        assertEquals("ABCD", result.getGameCode());
        assertEquals("01", result.getMaker());
        assertEquals(2, result.getVersion());
        assertEquals("", result.getErrorCode());
    }

    @Test
    public void statsDefaultsOnInvalidJson() {
        NativeStats stats = NativeStats.Companion.fromJson("not-json");

        assertEquals(0L, stats.getFrames());
        assertEquals(0, stats.getVideoWidth());
        assertEquals(0, stats.getVideoHeight());
        assertEquals("RGB565", stats.getVideoPixelFormat());
        assertEquals(0L, stats.getFrameTargetUs());
        assertEquals(0L, stats.getFrameActualUs());
        assertEquals(0L, stats.getFrameJitterUs());
        assertEquals(0L, stats.getFrameLateUs());
        assertEquals(0L, stats.getFramePacingSamples());
        assertFalse(stats.getRunning());
        assertTrue(stats.getPaused());
        assertEquals(100, stats.getVolumePercent());
        assertEquals(1024, stats.getAudioBufferSamples());
        assertFalse(stats.getAudioStarted());
        assertTrue(stats.getAudioPaused());
        assertTrue(stats.getAudioEnabled());
        assertEquals(0L, stats.getAudioEnqueuedBuffers());
        assertEquals(0L, stats.getAudioEnqueuedOutputFrames());
        assertEquals(0L, stats.getAudioReadFrames());
        assertEquals(0L, stats.getAudioLastReadFrames());
        assertEquals("OpenSL ES", stats.getAudioBackend());
        assertEquals(0, stats.getInputKeys());
        assertEquals(0, stats.getSeenInputKeys());
        assertFalse(stats.getGdbStubSupported());
        assertFalse(stats.getGdbStubEnabled());
        assertEquals(0, stats.getGdbStubPort());
    }

    @Test
    public void statsCoercesInvalidRanges() {
        NativeStats stats = NativeStats.Companion.fromJson(
            "{"
                + "\"videoPixelFormat\":\"RGB565\","
                + "\"frameTargetUs\":16667,"
                + "\"frameActualUs\":16720,"
                + "\"frameJitterUs\":53,"
                + "\"frameLateUs\":20,"
                + "\"framePacingSamples\":120,"
                + "\"fastForwardMultiplier\":99,"
                + "\"volumePercent\":200,"
                + "\"audioBufferSamples\":64,"
                + "\"audioStarted\":true,"
                + "\"audioPaused\":false,"
                + "\"audioEnabled\":false,"
                + "\"audioEnqueuedBuffers\":12,"
                + "\"audioEnqueuedOutputFrames\":9600,"
                + "\"audioReadFrames\":8800,"
                + "\"audioLastReadFrames\":800,"
                + "\"audioBackend\":\"AAudio\","
                + "\"audioLowPassRange\":120,"
                + "\"inputKeys\":2049,"
                + "\"seenInputKeys\":1073,"
                + "\"gdbStubSupported\":true,"
                + "\"gdbStubEnabled\":true,"
                + "\"gdbStubPort\":70000"
                + "}"
        );

        assertEquals("RGB565", stats.getVideoPixelFormat());
        assertEquals(16667L, stats.getFrameTargetUs());
        assertEquals(16720L, stats.getFrameActualUs());
        assertEquals(53L, stats.getFrameJitterUs());
        assertEquals(20L, stats.getFrameLateUs());
        assertEquals(120L, stats.getFramePacingSamples());
        assertEquals(200, stats.getFastForwardMultiplier());
        assertEquals(100, stats.getVolumePercent());
        assertEquals(512, stats.getAudioBufferSamples());
        assertTrue(stats.getAudioStarted());
        assertFalse(stats.getAudioPaused());
        assertFalse(stats.getAudioEnabled());
        assertEquals(12L, stats.getAudioEnqueuedBuffers());
        assertEquals(9600L, stats.getAudioEnqueuedOutputFrames());
        assertEquals(8800L, stats.getAudioReadFrames());
        assertEquals(800L, stats.getAudioLastReadFrames());
        assertEquals("AAudio", stats.getAudioBackend());
        assertEquals(95, stats.getAudioLowPassRange());
        assertEquals(1, stats.getInputKeys());
        assertEquals(49, stats.getSeenInputKeys());
        assertTrue(stats.getGdbStubSupported());
        assertTrue(stats.getGdbStubEnabled());
        assertEquals(65535, stats.getGdbStubPort());
    }

    @Test
    public void parsesGdbStubResultJson() {
        NativeGdbStubResult result = NativeGdbStubResult.Companion.fromJson(
            "{"
                + "\"ok\":true,"
                + "\"supported\":true,"
                + "\"enabled\":true,"
                + "\"port\":2345,"
                + "\"message\":\"GDB stub listening\""
                + "}"
        );

        assertTrue(result.getOk());
        assertTrue(result.getSupported());
        assertTrue(result.getEnabled());
        assertEquals(2345, result.getPort());
        assertEquals("GDB stub listening", result.getMessage());
    }
}
