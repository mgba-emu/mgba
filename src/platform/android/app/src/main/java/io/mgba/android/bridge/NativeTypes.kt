package io.mgba.android.bridge

import org.json.JSONObject

data class NativeLoadResult(
    val ok: Boolean,
    val message: String,
    val platform: String,
    val title: String,
    val displayName: String,
) {
    companion object {
        fun fromJson(raw: String): NativeLoadResult {
            val json = JSONObject(raw)
            return NativeLoadResult(
                ok = json.optBoolean("ok", false),
                message = json.optString("message"),
                platform = json.optString("platform"),
                title = json.optString("title"),
                displayName = json.optString("displayName"),
            )
        }
    }
}
