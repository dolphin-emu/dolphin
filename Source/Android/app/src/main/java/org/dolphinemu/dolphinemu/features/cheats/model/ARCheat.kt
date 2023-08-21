// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.model

import androidx.annotation.Keep

class ARCheat : AbstractCheat {
    @Keep
    private val pointer: Long

    constructor() {
        pointer = createNew()
    }

    @Keep
    private constructor(pointer: Long) {
        this.pointer = pointer
    }

    external fun finalize()

    private external fun createNew(): Long

    override fun supportsCreator(): Boolean = false

    override fun supportsNotes(): Boolean = false

    external override fun getName(): String

    external override fun getCode(): String

    external override fun getUserDefined(): Boolean

    external override fun getEnabled(): Boolean

    external override fun setEnabledImpl(enabled: Boolean)

    external override fun setCheatImpl(
        name: String,
        creator: String,
        notes: String,
        code: String
    ): Int

    companion object {
        @JvmStatic
        external fun loadCodes(gameId: String, revision: Int): Array<ARCheat>

        @JvmStatic
        external fun saveCodes(gameId: String, revision: Int, codes: Array<ARCheat>)
    }
}
