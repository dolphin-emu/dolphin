// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.skylanders.model

class Skylander(id: Int, variant: Int, var name: String) {
    private val pair: SkylanderPair = SkylanderPair(id, variant)

    val id: Int get() = pair.id
    val variant: Int get() = pair.variant

    companion object {
        @JvmField
        val BLANK_SKYLANDER = Skylander(-1, -1, "Blank")
    }
}
