
package org.mgba_emu.mgba.utils

import android.util.Log
import android.util.Xml
import com.github.luben.zstd.ZstdInputStream
import org.mgba_emu.mgba.mGBAApplication
import org.xmlpull.v1.XmlPullParser

object NoIntroParser {
    private const val TAG = "NoIntroParser"
    private val dbFiles = listOf("gb.dat.zst", "gbc.dat.zst", "gba.dat.zst")

    fun findTitle(gameCode: String): String? {
        for (fileName in dbFiles) {
            try {
                val title = findGameTitleByCode(fileName, gameCode)
                if (title != null) {
                    Log.d(TAG, "Match found in $fileName: $title")
                    return title
                } else {
                    Log.d(TAG, "Finished scanning $fileName. No match found.")
                }
            } catch (e: Exception) {
                Log.e(TAG, "CRASH while reading $fileName: ${e.message}", e)
            }
        }
        return null
    }

    private fun findGameTitleByCode(assetFileName: String, targetCode: String): String? {
        var gamesScanned = 0
        mGBAApplication.context.assets.open(assetFileName).use { assetStream ->
            ZstdInputStream(assetStream).use { zstdStream ->
                val parser: XmlPullParser = Xml.newPullParser()
                parser.setFeature(XmlPullParser.FEATURE_PROCESS_NAMESPACES, false)
                parser.setInput(zstdStream, null)

                var eventType = parser.eventType
                var currentGameName: String? = null

                while (eventType != XmlPullParser.END_DOCUMENT) {
                    if (eventType == XmlPullParser.START_TAG) {
                        val tagName = parser.name

                        if (tagName == "game") {
                            currentGameName = parser.getAttributeValue(null, "name")
                            gamesScanned++
                            if (gamesScanned == 1) {
                                Log.d(TAG, "Successfully read first game: $currentGameName")
                            }
                        }
                        else if (tagName == "rom") {
                            val serial = parser.getAttributeValue(null, "serial")

                            if (serial != null) {
                                val shortTargetCode = if (targetCode.length >= 4) targetCode.takeLast(4) else targetCode
                                if (serial.equals(shortTargetCode, ignoreCase = true)) {
                                    return currentGameName
                                }
                            }
                        }
                    }
                    eventType = parser.next()
                }
                Log.d(TAG, "Total games scanned in $assetFileName: $gamesScanned")
            }
        }
        return null
    }
}