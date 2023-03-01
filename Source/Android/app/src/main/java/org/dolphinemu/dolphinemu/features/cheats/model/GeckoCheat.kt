// SPDX-License-Identifier: GPL-2.0-or-later
package org.dolphinemu.dolphinemu.features.cheats.model

import androidx.annotation.Keep

class GeckoCheat : AbstractCheat {
    @Keep
    private val mPointer: Long

    constructor() {
        mPointer = createNew()
    }

    @Keep
    private constructor(pointer: Long) {
        mPointer = pointer
    }

    external fun finalize()

    private external fun createNew(): Long

    override fun equals(other: Any?): Boolean {
        return other != null && javaClass == other.javaClass && equalsImpl(other as GeckoCheat)
    }

    override fun hashCode(): Int {
        return mPointer.hashCode()
    }

    override fun supportsCreator(): Boolean {
        return true
    }

    override fun supportsNotes(): Boolean {
        return true
    }

    external override fun getName(): String

    external override fun getCreator(): String

    external override fun getNotes(): String

    external override fun getCode(): String

    external override fun getUserDefined(): Boolean

    external override fun getEnabled(): Boolean

    private external fun equalsImpl(other: GeckoCheat): Boolean

    external override fun setCheatImpl(
        name: String,
        creator: String,
        notes: String,
        code: String
    ): Int

    external override fun setEnabledImpl(enabled: Boolean)

    companion object {
        @JvmStatic
        external fun loadCodes(gameId: String, revision: Int): Array<GeckoCheat>

        @JvmStatic
        external fun saveCodes(gameId: String, revision: Int, codes: Array<GeckoCheat>)

        @JvmStatic
        external fun downloadCodes(gameTdbId: String): Array<GeckoCheat>?
    }
}