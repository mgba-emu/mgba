package io.mgba.android;

import android.content.Context;
import android.os.ParcelFileDescriptor;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import io.mgba.android.bridge.NativeBridge;
import io.mgba.android.bridge.NativeLoadResult;
import io.mgba.android.bridge.NativeStats;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

import org.junit.Test;
import org.junit.runner.RunWith;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

@RunWith(AndroidJUnit4.class)
public final class NativeSmokeInstrumentedTest {
    private static final String ROM_ASSET_NAME = "native-smoke-dmg-acid2.gb";
    private static final int SMOKE_STATE_SLOT = 9;

    @Test
    public void testNativeCoreLoadsRunsAndRestoresPublicDomainRom() throws Exception {
        Context testContext = InstrumentationRegistry.getInstrumentation().getContext();
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        File rom = copyAssetToCache(testContext, context, ROM_ASSET_NAME);

        try (ParcelFileDescriptor descriptor = ParcelFileDescriptor.open(rom, ParcelFileDescriptor.MODE_READ_ONLY)) {
            NativeLoadResult probe = NativeLoadResult.Companion.fromJson(
                NativeBridge.nativeProbeRomFd(descriptor.getFd(), ROM_ASSET_NAME)
            );
            assertTrue(probe.getMessage(), probe.getOk());
            assertEquals("GB", probe.getPlatform());
        }

        long handle = NativeBridge.nativeCreate(
            context.getFilesDir().getAbsolutePath(),
            context.getCacheDir().getAbsolutePath()
        );
        assertTrue("Native handle should be valid", handle != 0L);

        try {
            try (ParcelFileDescriptor descriptor = ParcelFileDescriptor.open(rom, ParcelFileDescriptor.MODE_READ_ONLY)) {
                NativeLoadResult load = NativeBridge.INSTANCE.loadRomFd(handle, descriptor.getFd(), ROM_ASSET_NAME);
                assertTrue(load.getMessage(), load.getOk());
                assertEquals("GB", load.getPlatform());
            }

            for (int frame = 0; frame < 300; ++frame) {
                assertTrue("Frame " + frame + " should run", NativeBridge.nativeStepFrame(handle));
            }

            NativeStats stats = NativeBridge.INSTANCE.stats(handle);
            assertTrue("Frame counter should advance", stats.getFrames() >= 300L);
            assertEquals("GB", stats.getRomPlatform());
            assertEquals("RGB565", stats.getVideoPixelFormat());

            assertTrue("State save should succeed", NativeBridge.nativeSaveStateSlot(handle, SMOKE_STATE_SLOT));
            assertTrue("State should exist", NativeBridge.nativeHasStateSlot(handle, SMOKE_STATE_SLOT));

            for (int frame = 0; frame < 10; ++frame) {
                assertTrue("Post-save frame " + frame + " should run", NativeBridge.nativeStepFrame(handle));
            }

            assertTrue("State load should succeed", NativeBridge.nativeLoadStateSlot(handle, SMOKE_STATE_SLOT));
            assertTrue("State cleanup should succeed", NativeBridge.nativeDeleteStateSlot(handle, SMOKE_STATE_SLOT));
        } finally {
            NativeBridge.nativeDestroy(handle);
        }
    }

    private File copyAssetToCache(Context assetContext, Context targetContext, String assetName) throws Exception {
        File target = new File(targetContext.getCacheDir(), assetName);
        try (
            InputStream input = assetContext.getAssets().open(assetName);
            FileOutputStream output = new FileOutputStream(target, false)
        ) {
            byte[] buffer = new byte[8192];
            int read;
            while ((read = input.read(buffer)) != -1) {
                output.write(buffer, 0, read);
            }
        }
        return target;
    }
}
