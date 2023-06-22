// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.model

import org.dolphinemu.dolphinemu.features.cheats.model.Cheat.Companion.TRY_SET_FAIL_NO_NAME
import org.dolphinemu.dolphinemu.features.cheats.model.Cheat.Companion.TRY_SET_SUCCESS

abstract class AbstractCheat : ReadOnlyCheat() {
    override fun supportsCode(): Boolean {
        return true
    }

    override fun setCheat(
        name: String,
        creator: String,
        notes: String,
        code: String
    ): Int {
        var finalName = name
        var finalCode = code
        if (finalCode.isNotEmpty() && finalCode[0] == '$') {
            val firstLineEnd = finalCode.indexOf('\n')
            if (firstLineEnd == -1) {
                finalName = finalCode.substring(1)
                finalCode = ""
            } else {
                finalName = finalCode.substring(1, firstLineEnd)
                finalCode = finalCode.substring(firstLineEnd + 1)
            }
        }

        if (finalName.isEmpty()) return TRY_SET_FAIL_NO_NAME

        val result = setCheatImpl(finalName, creator, notes, finalCode)

        if (result == TRY_SET_SUCCESS) onChanged()

        return result
    }

    protected abstract fun setCheatImpl(
        name: String,
        creator: String,
        notes: String,
        code: String
    ): Int
}
