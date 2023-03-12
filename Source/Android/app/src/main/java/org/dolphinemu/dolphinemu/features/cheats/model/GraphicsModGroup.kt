package org.dolphinemu.dolphinemu.features.cheats.model

import androidx.annotation.Keep

class GraphicsModGroup @Keep private constructor(@field:Keep private val pointer: Long) {
    external fun finalize()

    val mods: Array<GraphicsMod>
        external get

    external fun save()

    companion object {
        @JvmStatic
        external fun load(gameId: String): GraphicsModGroup
    }
}
