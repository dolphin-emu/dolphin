// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import androidx.annotation.Keep

@Keep
fun interface CompressCallback {
    fun run(text: String, completion: Float): Boolean
}
