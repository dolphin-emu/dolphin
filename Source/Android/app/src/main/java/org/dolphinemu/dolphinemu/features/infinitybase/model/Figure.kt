// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.infinitybase.model

data class Figure(var number: Long, var name: String) {

    companion object {
        @JvmField
        val BLANK_FIGURE = Figure(-1, "Blank")
    }
}
