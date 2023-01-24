// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

object WiiUtils {
    const val RESULT_SUCCESS = 0
    const val RESULT_ERROR = 1
    const val RESULT_CANCELLED = 2
    const val RESULT_CORRUPTED_SOURCE = 3
    const val RESULT_TITLE_MISSING = 4
    const val UPDATE_RESULT_SUCCESS = 0
    const val UPDATE_RESULT_ALREADY_UP_TO_DATE = 1
    const val UPDATE_RESULT_REGION_MISMATCH = 2
    const val UPDATE_RESULT_MISSING_UPDATE_PARTITION = 3
    const val UPDATE_RESULT_DISC_READ_FAILED = 4
    const val UPDATE_RESULT_SERVER_FAILED = 5
    const val UPDATE_RESULT_DOWNLOAD_FAILED = 6
    const val UPDATE_RESULT_IMPORT_FAILED = 7
    const val UPDATE_RESULT_CANCELLED = 8

    @JvmStatic
    external fun installWAD(file: String): Boolean

    @JvmStatic
    external fun importWiiSave(file: String, canOverwrite: BooleanSupplier): Int

    @JvmStatic
    external fun importNANDBin(file: String)
    external fun doOnlineUpdate(region: String, callback: WiiUpdateCallback): Int
    external fun doDiscUpdate(path: String, callback: WiiUpdateCallback): Int

    @JvmStatic
    external fun isSystemMenuInstalled(): Boolean

    @JvmStatic
    external fun isSystemMenuvWii(): Boolean

    @JvmStatic
    external fun getSystemMenuVersion(): String

    @JvmStatic
    external fun syncSdFolderToSdImage(): Boolean

    @JvmStatic
    external fun syncSdImageToSdFolder(): Boolean
}
