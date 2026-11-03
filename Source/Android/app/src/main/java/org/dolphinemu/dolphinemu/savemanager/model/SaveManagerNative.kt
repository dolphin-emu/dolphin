// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.savemanager.model

object SaveManagerNative {
    // GameCube methods
    @JvmStatic
    external fun getGCSaveFiles(path: String): Array<GCSaveFile>

    @JvmStatic
    external fun getGCMemcardStats(path: String): GCMemcardStats?

    @JvmStatic
    external fun copyGCSaveFile(srcPath: String, index: Int, dstPath: String): Boolean

    @JvmStatic
    external fun deleteGCSaveFile(path: String, index: Int): Boolean

    @JvmStatic
    external fun exportGCSaveFile(srcPath: String, index: Int, dstPath: String): Boolean

    @JvmStatic
    external fun importGCSaveFile(srcPath: String, dstPath: String): Boolean

    @JvmStatic
    external fun fixGCChecksums(path: String): Boolean

    // Wii methods
    @JvmStatic
    external fun getWiiSaveFiles(): Array<WiiSaveFile>

    @JvmStatic
    external fun deleteWiiSaveFile(titleId: Long): Boolean

    @JvmStatic
    external fun exportWiiSaveFile(titleId: Long, dstPath: String): Int

    @JvmStatic
    external fun importWiiSaveFile(srcPath: String, overwrite: Boolean): Int

    @JvmStatic
    external fun getWiiSaveTitleId(srcPath: String): Long
}
