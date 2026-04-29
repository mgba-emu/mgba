package io.mgba.android.bridge

@JvmInline
value class EmulatorHandle(val value: Long) {
    val isValid: Boolean
        get() = value != 0L
}
