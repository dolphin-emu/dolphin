// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui

import org.dolphinemu.dolphinemu.R

enum class Tab(private val value: Int) {
    GAMECUBE(0),
    WII(1),
    WIIWARE(2),
    MORE(3);

    fun toInt(): Int {
        return value
    }

    fun getID(): Int {
        when (value) {
            GAMECUBE.toInt() -> return R.id.gamecubeFragment
            WII.toInt() -> return R.id.wiiFragment
            WIIWARE.toInt() -> return R.id.wiiwareFragment
            MORE.toInt() -> return R.id.moreFragment
        }
        return 0
    }

    companion object {
        fun fromInt(i: Int): Tab {
            return Tab.values()[i]
        }
    }
}
