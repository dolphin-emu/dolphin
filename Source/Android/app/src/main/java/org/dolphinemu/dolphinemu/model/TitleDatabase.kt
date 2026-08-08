// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.model

import androidx.annotation.Keep

@Keep
class TitleDatabase private constructor(private val pointer: Long) {
    external fun finalize()

    external fun areUserTitleMapsEqual(other: TitleDatabase?): Boolean

    companion object {
        @JvmStatic
        external suspend fun load(): TitleDatabase
    }
}
