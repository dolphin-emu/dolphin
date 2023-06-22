// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.model

interface Cheat {
    fun supportsCreator(): Boolean

    fun supportsNotes(): Boolean

    fun supportsCode(): Boolean

    fun getName(): String = ""

    fun getCreator(): String = ""

    fun getNotes(): String = ""

    fun getCode(): String = ""

    fun setCheat(
        name: String,
        creator: String,
        notes: String,
        code: String
    ): Int

    fun getUserDefined(): Boolean

    fun getEnabled(): Boolean

    fun setEnabled(isChecked: Boolean)

    fun setChangedCallback(callback: Runnable?)

    companion object {
        // Result codes greater than 0 represent an error on the corresponding code line (one-indexed)
        const val TRY_SET_FAIL_CODE_MIXED_ENCRYPTION = -3
        const val TRY_SET_FAIL_NO_CODE_LINES = -2
        const val TRY_SET_FAIL_NO_NAME = -1
        const val TRY_SET_SUCCESS = 0
    }
}
