// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.platform

import org.dolphinemu.dolphinemu.R

/**
 * Enum to represent platform tabs in the UI.
 *
 * Each platform tab corresponds to one or more platforms.
 */
enum class PlatformTab(private val value: Int, val headerName: Int, val idString: String) {
    GAMECUBE(0, R.string.platform_gamecube, "GameCube Games"),
    WII(1, R.string.platform_wii, "Wii Games"),
    WIIWARE(2, R.string.platform_wiiware, "WiiWare Games");

    fun toInt(): Int {
        return value
    }

    companion object {
        fun fromInt(i: Int): PlatformTab {
            return values()[i]
        }

        fun fromPosition(position: Int): PlatformTab {
            return values()[position]
        }
    }
}
