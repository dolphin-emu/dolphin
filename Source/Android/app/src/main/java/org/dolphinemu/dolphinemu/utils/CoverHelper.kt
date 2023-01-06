// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import org.dolphinemu.dolphinemu.model.GameFile
import android.graphics.Bitmap
import java.io.FileOutputStream
import java.lang.Exception

object CoverHelper {
    fun buildGameTDBUrl(game: GameFile, region: String?): String {
        val baseUrl = "https://art.gametdb.com/wii/cover/%s/%s.png"
        return String.format(baseUrl, region, game.gameTdbId)
    }

    fun getRegion(game: GameFile): String {
        val region: String = when (game.region) {
            0 -> "JA"
            1 -> "US"
            4 -> "KO"
            2 -> when (game.country) {
                3 -> "AU"
                4 -> "FR"
                5 -> "DE"
                6 -> "IT"
                8 -> "NL"
                9 -> "RU"
                10 -> "ES"
                0 -> "EN"
                else -> "EN"
            }
            3 -> "EN"
            else -> "EN"
        }
        return region
    }

    fun saveCover(cover: Bitmap, path: String?) {
        try {
            val out = FileOutputStream(path)
            cover.compress(Bitmap.CompressFormat.PNG, 100, out)
            out.close()
        } catch (ignored: Exception) {
        }
    }
}
