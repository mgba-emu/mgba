package io.mgba.android.input

data class GbaButton(
    val mask: Int,
    val label: String,
)

object GbaButtons {
    val All = listOf(
        GbaButton(GbaKeyMask.A, "A"),
        GbaButton(GbaKeyMask.B, "B"),
        GbaButton(GbaKeyMask.L, "L"),
        GbaButton(GbaKeyMask.R, "R"),
        GbaButton(GbaKeyMask.Select, "Select"),
        GbaButton(GbaKeyMask.Start, "Start"),
        GbaButton(GbaKeyMask.Up, "Up"),
        GbaButton(GbaKeyMask.Down, "Down"),
        GbaButton(GbaKeyMask.Left, "Left"),
        GbaButton(GbaKeyMask.Right, "Right"),
    )

    fun labelForMask(mask: Int): String {
        return All.firstOrNull { it.mask == mask }?.label ?: "Unknown"
    }
}
