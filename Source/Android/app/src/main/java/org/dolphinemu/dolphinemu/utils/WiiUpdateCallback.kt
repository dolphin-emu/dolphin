// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import androidx.annotation.Keep

@Keep
fun interface WiiUpdateCallback {
    fun run(processed: Int, total: Int, titleId: Long): Boolean
}
