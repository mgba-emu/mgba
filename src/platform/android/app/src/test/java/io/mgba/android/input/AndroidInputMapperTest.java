package io.mgba.android.input;

import static org.junit.Assert.assertEquals;

import android.view.InputDevice;
import android.view.KeyEvent;
import org.junit.Test;

public class AndroidInputMapperTest {
    @Test
    public void mapsGamepadFaceButtons() {
        assertEquals(GbaKeyMask.A, AndroidInputMapper.INSTANCE.keyMaskForKeyCode(KeyEvent.KEYCODE_BUTTON_A));
        assertEquals(GbaKeyMask.B, AndroidInputMapper.INSTANCE.keyMaskForKeyCode(KeyEvent.KEYCODE_BUTTON_B));
    }

    @Test
    public void customProfileReplacesDefaultTargetBinding() {
        HardwareKeyProfile profile = HardwareKeyProfile.defaultProfile()
            .withKeyCode(GbaKeyMask.B, KeyEvent.KEYCODE_BUTTON_A);

        assertEquals(GbaKeyMask.B, AndroidInputMapper.INSTANCE.keyMaskForKeyCode(KeyEvent.KEYCODE_BUTTON_A, profile));
        assertEquals(0, AndroidInputMapper.INSTANCE.keyMaskForKeyCode(KeyEvent.KEYCODE_BUTTON_B, profile));
        assertEquals(GbaKeyMask.A, AndroidInputMapper.INSTANCE.keyMaskForKeyCode(KeyEvent.KEYCODE_X, profile));
    }

    @Test
    public void customProfileKeepsLastBindingWhenKeyCodeConflicts() {
        HardwareKeyProfile profile = HardwareKeyProfile.defaultProfile()
            .withKeyCode(GbaKeyMask.A, KeyEvent.KEYCODE_SPACE)
            .withKeyCode(GbaKeyMask.B, KeyEvent.KEYCODE_SPACE);

        assertEquals(GbaKeyMask.B, AndroidInputMapper.INSTANCE.keyMaskForKeyCode(KeyEvent.KEYCODE_SPACE, profile));
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

    @Test
    public void mapsJoystickAxesAndHatDirections() {
        int source = InputDevice.SOURCE_JOYSTICK;

        assertEquals(
            GbaKeyMask.Left | GbaKeyMask.Up,
            AndroidInputMapper.INSTANCE.motionKeysForAxes(
                source,
                -0.6f,
                -0.5f,
                0f,
                0f,
                0f,
                0f,
                0f,
                0f,
                45
            )
        );
        assertEquals(
            GbaKeyMask.Right | GbaKeyMask.Down,
            AndroidInputMapper.INSTANCE.motionKeysForAxes(
                source,
                0f,
                0f,
                1f,
                1f,
                0f,
                0f,
                0f,
                0f,
                45
            )
        );
    }

    @Test
    public void mapsTriggerAxesToShoulderButtons() {
        int source = InputDevice.SOURCE_GAMEPAD;

        assertEquals(
            GbaKeyMask.L | GbaKeyMask.R,
            AndroidInputMapper.INSTANCE.motionKeysForAxes(
                source,
                0f,
                0f,
                0f,
                0f,
                0.5f,
                0f,
                0.2f,
                0.8f,
                45
            )
        );
    }

    @Test
    public void clampsAxisThreshold() {
        int source = InputDevice.SOURCE_GAMEPAD;

        assertEquals(
            GbaKeyMask.Right,
            AndroidInputMapper.INSTANCE.motionKeysForAxes(
                source,
                0.2f,
                0f,
                0f,
                0f,
                0f,
                0f,
                0f,
                0f,
                5
            )
        );
        assertEquals(
            0,
            AndroidInputMapper.INSTANCE.motionKeysForAxes(
                source,
                0.2f,
                0f,
                0f,
                0f,
                0f,
                0f,
                0f,
                0f,
                95
            )
        );
    }

    @Test
    public void ignoresMotionFromNonGamepadSources() {
        assertEquals(
            0,
            AndroidInputMapper.INSTANCE.motionKeysForAxes(
                InputDevice.SOURCE_KEYBOARD,
                1f,
                1f,
                1f,
                1f,
                1f,
                1f,
                1f,
                1f,
                45
            )
        );
    }
}
