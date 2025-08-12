// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.platform

import org.dolphinemu.dolphinemu.R

/**
 * Enum to represent platform (eg GameCube, Wii).
 */
enum class Platform(private val value: Int) {
    GAMECUBE(0),
    TRIFORCE(1),
    WII(2),
    WIIWARE(3),
    ELF_DOL(4);

    fun toInt(): Int {
        return value
    }

    /**
     * Returns which UI tab this platform should be shown in.
     */
    fun toPlatformTab(): PlatformTab {
        return when (this) {
            GAMECUBE -> PlatformTab.GAMECUBE
            TRIFORCE -> PlatformTab.GAMECUBE
            WII -> PlatformTab.WII
            WIIWARE -> PlatformTab.WIIWARE
            ELF_DOL -> PlatformTab.WIIWARE
        }
    }

    companion object {
        @JvmStatic
        fun fromInt(i: Int): Platform {
            return values()[i]
        }
    }
}
