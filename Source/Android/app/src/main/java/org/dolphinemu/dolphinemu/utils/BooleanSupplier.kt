// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import androidx.annotation.Keep

/**
 * java.util.function.BooleanSupplier is only provided starting with API 24, while Dolphin still supports API
 * 21. API desugaring lets Kotlin/Java code use it on older Android versions, but JNI `FindClass("java/util/
 * function/BooleanSupplier")` isn’t desugared, so native code can’t look up that type reliably on API 23 and
 * below. Because of that limitation, we keep our own interface instead of relying on the platform type.
 */
@Keep
fun interface BooleanSupplier {
    fun get(): Boolean
}
