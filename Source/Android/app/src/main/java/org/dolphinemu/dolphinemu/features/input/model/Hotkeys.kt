// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model

object Hotkeys {
    /**
     * Stops or resumes the native hotkey scheduler acting on presses.
     */
    @JvmStatic
    external fun setEnabled(enabled: Boolean)
}
