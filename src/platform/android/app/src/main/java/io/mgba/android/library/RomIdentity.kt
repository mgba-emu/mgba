package io.mgba.android.library

import java.util.Locale

object RomIdentity {
    fun stableGameId(gameId: String, crc32: String, sha1: String): String {
        val normalizedSha1 = normalizedSha1(sha1)
        if (normalizedSha1.isNotBlank()) {
            return "sha1:$normalizedSha1"
        }
        return crc32GameId(gameId, crc32)
    }

    fun crc32GameId(gameId: String, crc32: String): String {
        val normalizedCrc32 = normalizedCrc32(crc32)
        return if (normalizedCrc32.isBlank()) gameId else "crc32:$normalizedCrc32"
    }

    fun normalizedCrc32(value: String): String {
        return value.trim().lowercase(Locale.US)
    }

    fun normalizedSha1(value: String): String {
        return value.trim().lowercase(Locale.US)
    }
}
