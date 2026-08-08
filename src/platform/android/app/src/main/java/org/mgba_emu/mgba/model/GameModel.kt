
package org.mgba_emu.mgba.model

import android.net.Uri

data class GameModel(
    val uri: Uri,
    val fileName: String,
    var title: String? = null,
    var version: String? = null,
    var iconUrl: String? = null,
    var code: String? = null,
    var lastPlayed: Long = 0L
)