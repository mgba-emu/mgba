package io.mgba.android;

import android.content.Context;
import android.view.MotionEvent;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import io.mgba.android.input.GbaKeyMask;
import io.mgba.android.input.VirtualGamepadView;
import kotlin.Unit;

import org.junit.Test;
import org.junit.runner.RunWith;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

@RunWith(AndroidJUnit4.class)
public final class VirtualGamepadInstrumentedTest {
    @Test
    public void touchEventsReportEventTimeAndMappedKeys() {
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        final int[] seenKeys = {0};
        final long[] seenEventTimeMs = {-1L};
        final long eventTimeMs = 123456L;

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            VirtualGamepadView view = new VirtualGamepadView(context);
            view.layout(0, 0, 1080, 1920);
            view.setOnKeysChangedListener((keys, reportedEventTimeMs) -> {
                seenKeys[0] = keys;
                seenEventTimeMs[0] = reportedEventTimeMs;
                return Unit.INSTANCE;
            });

            MotionEvent down = MotionEvent.obtain(
                eventTimeMs,
                eventTimeMs,
                MotionEvent.ACTION_DOWN,
                872f,
                1580f,
                0
            );
            try {
                assertTrue(view.onTouchEvent(down));
            } finally {
                down.recycle();
            }
        });

        assertTrue("Virtual face button touch should include GBA A", (seenKeys[0] & GbaKeyMask.A) != 0);
        assertEquals("Virtual input should preserve the originating event time", eventTimeMs, seenEventTimeMs[0]);
    }
}
