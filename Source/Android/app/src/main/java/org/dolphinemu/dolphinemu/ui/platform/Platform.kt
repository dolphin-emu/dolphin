// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.platform

import org.dolphinemu.dolphinemu.R

/**
 * Enum to represent platform (eg GameCube, Wii).
 */
enum class Platform(private val value: Int, val headerName: Int, val idString: String) {
    GAMECUBE(0, R.string.platform_gamecube, "GameCube Games"),
    WII(1, R.string.platform_wii, "Wii Games"),
    WIIWARE(2, R.string.platform_wiiware, "WiiWare Games");

    fun toInt(): Int {
        return value
    }

    companion object {
        fun fromInt(i: Int): Platform {
            return values()[i]
        }

        @JvmStatic
        fun fromNativeInt(i: Int): Platform {
            // TODO: Proper support for DOL and ELF files
            val inRange = i >= 0 && i < values().size
            return values()[if (inRange) i else WIIWARE.value]
        }

        @JvmStatic
        fun fromPosition(position: Int): Platform {
            return values()[position]
        }
    }
}
