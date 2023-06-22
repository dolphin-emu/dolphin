// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.model

import androidx.annotation.Keep

class GraphicsMod @Keep private constructor(
    @Keep private val pointer: Long,
    // When a C++ GraphicsModGroup object is destroyed, it also destroys the GraphicsMods it owns.
    // To avoid getting dangling pointers, we keep a reference to the GraphicsModGroup here.
    @Keep private val parent: GraphicsModGroup
) : ReadOnlyCheat() {
    override fun supportsCreator(): Boolean = true

    override fun supportsNotes(): Boolean = true

    override fun supportsCode(): Boolean = false

    external override fun getName(): String

    external override fun getCreator(): String

    external override fun getNotes(): String

    // Technically graphics mods can be user defined, but we don't support editing graphics mods
    // in the GUI, and editability is what this really controls
    override fun getUserDefined(): Boolean = false

    external override fun getEnabled(): Boolean

    external override fun setEnabledImpl(enabled: Boolean)
}
