package io.mgba.android.input;

import static org.junit.Assert.assertEquals;

import android.view.KeyEvent;
import org.junit.Test;

public class AndroidInputMapperTest {
    @Test
    public void mapsGamepadFaceButtons() {
        assertEquals(GbaKeyMask.A, AndroidInputMapper.INSTANCE.keyMaskForKeyCode(KeyEvent.KEYCODE_BUTTON_A));
        assertEquals(GbaKeyMask.B, AndroidInputMapper.INSTANCE.keyMaskForKeyCode(KeyEvent.KEYCODE_BUTTON_B));
    }

    @Test
    public void mapsKeyboardFallbackButtons() {
        assertEquals(GbaKeyMask.A, AndroidInputMapper.INSTANCE.keyMaskForKeyCode(KeyEvent.KEYCODE_X));
        assertEquals(GbaKeyMask.B, AndroidInputMapper.INSTANCE.keyMaskForKeyCode(KeyEvent.KEYCODE_Z));
        assertEquals(GbaKeyMask.L, AndroidInputMapper.INSTANCE.keyMaskForKeyCode(KeyEvent.KEYCODE_Q));
        assertEquals(GbaKeyMask.R, AndroidInputMapper.INSTANCE.keyMaskForKeyCode(KeyEvent.KEYCODE_E));
    }

    @Test
    public void mapsDirectionalAndMenuKeys() {
        assertEquals(GbaKeyMask.Up, AndroidInputMapper.INSTANCE.keyMaskForKeyCode(KeyEvent.KEYCODE_DPAD_UP));
        assertEquals(GbaKeyMask.Down, AndroidInputMapper.INSTANCE.keyMaskForKeyCode(KeyEvent.KEYCODE_DPAD_DOWN));
        assertEquals(GbaKeyMask.Left, AndroidInputMapper.INSTANCE.keyMaskForKeyCode(KeyEvent.KEYCODE_DPAD_LEFT));
        assertEquals(GbaKeyMask.Right, AndroidInputMapper.INSTANCE.keyMaskForKeyCode(KeyEvent.KEYCODE_DPAD_RIGHT));
        assertEquals(GbaKeyMask.Select, AndroidInputMapper.INSTANCE.keyMaskForKeyCode(KeyEvent.KEYCODE_BUTTON_SELECT));
        assertEquals(GbaKeyMask.Start, AndroidInputMapper.INSTANCE.keyMaskForKeyCode(KeyEvent.KEYCODE_BUTTON_START));
    }

    @Test
    public void ignoresUnknownKeys() {
        assertEquals(0, AndroidInputMapper.INSTANCE.keyMaskForKeyCode(KeyEvent.KEYCODE_SPACE));
    }
}
