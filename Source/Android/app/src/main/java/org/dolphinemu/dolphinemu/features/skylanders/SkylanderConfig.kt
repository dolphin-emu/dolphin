// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.skylanders

import android.util.Pair
import org.dolphinemu.dolphinemu.features.skylanders.model.SkylanderPair

object SkylanderConfig {
    var LIST_SKYLANDERS: Map<SkylanderPair, String> = getSkylanderMap()
    var REVERSE_LIST_SKYLANDERS: Map<String, SkylanderPair> = getInverseSkylanderMap()

    private external fun getSkylanderMap(): Map<SkylanderPair, String>
    private external fun getInverseSkylanderMap(): Map<String, SkylanderPair>

    @JvmStatic
    external fun removeSkylander(slot: Int): Boolean

    @JvmStatic
    external fun loadSkylander(slot: Int, fileName: String?): Pair<Int?, String?>?

    @JvmStatic
    external fun createSkylander(
        id: Int,
        variant: Int,
        fileName: String,
        slot: Int
    ): Pair<Int, String>
}
