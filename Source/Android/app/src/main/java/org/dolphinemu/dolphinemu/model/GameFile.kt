// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.model

import androidx.annotation.Keep

@Keep
class GameFile private constructor(private val pointer: Long) {
    external fun finalize()

    external fun getPlatform(): Int

    external fun getTitle(): String

    external fun getDescription(): String

    external fun getCompany(): String

    external fun getCountry(): Int

    external fun getRegion(): Int

    external fun getPath(): String

    external fun getGameId(): String

    external fun getGameTdbId(): String

    external fun getDiscNumber(): Int

    external fun getRevision(): Int

    external fun getBlobType(): Int

    external fun getFileFormatName(): String

    external fun getBlockSize(): Long

    external fun getCompressionMethod(): String

    external fun shouldShowFileFormatDetails(): Boolean

    external fun shouldAllowConversion(): Boolean

    external fun getFileSize(): Long

    external fun isDatelDisc(): Boolean

    external fun isNKit(): Boolean

    external fun getBanner(): IntArray

    external fun getBannerWidth(): Int

    external fun getBannerHeight(): Int

    val customCoverPath: String
        get() = "${getPath().substring(0, getPath().lastIndexOf("."))}.cover.png"

    companion object {
        var REGION_NTSC_J = 0
        var REGION_NTSC_U = 1
        var REGION_PAL = 2
        var REGION_NTSC_K = 4

        @JvmStatic
        external fun parse(path: String): GameFile?
    }
}
