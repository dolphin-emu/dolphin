// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model

import androidx.annotation.Keep

/**
 * Calls the passed-in Runnable when Dolphin's config changes.
 *
 * Please note: The Runnable may be called from any thread.
 */
class ConfigChangedCallback(runnable: Runnable) {
    @Keep
    private var pointer: Long = initialize(runnable)

    /**
     * Stops the callback from being called in the future.
     */
    fun unregister() {
        if (pointer != 0L) {
            deinitialize(pointer)
            pointer = 0L
        }
    }

    companion object {
        @JvmStatic
        private external fun initialize(runnable: Runnable): Long

        @JvmStatic
        private external fun deinitialize(pointer: Long)
    }
}
