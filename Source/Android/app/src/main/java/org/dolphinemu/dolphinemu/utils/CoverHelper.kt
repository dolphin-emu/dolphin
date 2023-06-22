// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import org.dolphinemu.dolphinemu.model.GameFile

object CoverHelper {
    @JvmStatic
    fun buildGameTDBUrl(game: GameFile, region: String?): String {
        val baseUrl = "https://art.gametdb.com/wii/cover/%s/%s.png"
        return String.format(baseUrl, region, game.gameTdbId)
    }

    @JvmStatic
    fun getRegion(game: GameFile): String {
        val region: String = when (game.region) {
            GameFile.REGION_NTSC_J -> "JA"
            GameFile.REGION_NTSC_U -> "US"
            GameFile.REGION_NTSC_K -> "KO"
            GameFile.REGION_PAL -> when (game.country) {
                3 -> "AU" // Australia
                4 -> "FR" // France
                5 -> "DE" // Germany
                6 -> "IT" // Italy
                8 -> "NL" // Netherlands
                9 -> "RU" // Russia
                10 -> "ES" // Spain
                0 -> "EN" // Europe
                else -> "EN"
            }
            3 -> "EN" // Unknown
            else -> "EN"
        }
        return region
    }
}
