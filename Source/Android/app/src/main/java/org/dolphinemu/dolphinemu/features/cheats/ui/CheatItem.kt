// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui

import org.dolphinemu.dolphinemu.features.cheats.model.Cheat

class CheatItem {
    val cheat: Cheat?
    val string: Int
    val type: Int

    constructor(cheat: Cheat) {
        this.cheat = cheat
        string = 0
        type = TYPE_CHEAT
    }

    constructor(type: Int, string: Int) {
        cheat = null
        this.string = string
        this.type = type
    }

    companion object {
        const val TYPE_HEADER = 0
        const val TYPE_CHEAT = 1
        const val TYPE_ACTION = 2
    }
}
