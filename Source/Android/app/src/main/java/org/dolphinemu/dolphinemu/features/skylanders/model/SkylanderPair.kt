// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.skylanders.model

class SkylanderPair(var id: Int, var variant: Int) {
    override fun hashCode(): Int {
        return (id shl 16) + variant
    }

    override fun equals(other: Any?): Boolean {
        if (other !is SkylanderPair) return false
        if (other.id != id) return false
        return other.variant == variant
    }
}
