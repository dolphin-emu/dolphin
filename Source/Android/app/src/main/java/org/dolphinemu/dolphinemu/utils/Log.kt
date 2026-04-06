// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import org.dolphinemu.dolphinemu.BuildConfig
import android.util.Log as AndroidLog

/**
 * Contains methods that call through to [android.util.Log], but
 * with the same TAG automatically provided. Also no-ops VERBOSE and DEBUG log
 * levels in release builds.
 */
object Log {
    private const val TAG = "Dolphin"

    @JvmStatic
    fun verbose(message: String) {
        if (BuildConfig.DEBUG) {
            AndroidLog.v(TAG, message)
        }
    }

    @JvmStatic
    fun debug(message: String) {
        if (BuildConfig.DEBUG) {
            AndroidLog.d(TAG, message)
        }
    }

    @JvmStatic
    fun info(message: String) {
        AndroidLog.i(TAG, message)
    }

    @JvmStatic
    fun warning(message: String) {
        AndroidLog.w(TAG, message)
    }

    @JvmStatic
    fun error(message: String) {
        AndroidLog.e(TAG, message)
    }

    @JvmStatic
    fun wtf(message: String) {
        AndroidLog.wtf(TAG, message)
    }
}
