// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.infinitybase

object InfinityConfig {
    var LIST_FIGURES: Map<Long, String> = getFigureMap()
    var REVERSE_LIST_FIGURES: Map<String, Long> = getInverseFigureMap()

    private external fun getFigureMap(): Map<Long, String>
    private external fun getInverseFigureMap(): Map<String, Long>

    @JvmStatic
    external fun removeFigure(position: Int)

    @JvmStatic
    external fun loadFigure(position: Int, fileName: String): String?

    @JvmStatic
    external fun createFigure(
        figureNumber: Long,
        fileName: String,
        position: Int
    ): String?
}
