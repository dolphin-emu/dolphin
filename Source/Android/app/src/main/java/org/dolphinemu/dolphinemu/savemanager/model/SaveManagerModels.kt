// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.savemanager.model

import androidx.annotation.Keep

@Keep
data class GCSaveFile(
    val index: Int,
    val title: String,
    val subtitle: String,
    val gameId: String,
    val companyId: String,
    val region: Int,
    val blockSize: Int,
    val banner: IntArray?,
    val iconFrames: Array<IntArray>?,
    val iconDelay: Int
)

@Keep
data class GCMemcardStats(
    val freeBlocks: Int,
    val freeFiles: Int
)

@Keep
data class WiiSaveFile(
    val titleId: Long,
    val gameId: String,
    val title: String,
    val description: String,
    val region: Int,
    val banner: IntArray?
)

@Keep
data class GBASaveFile(
    val fileName: String,
    val filePath: String,
    val slot: Int,
    val gameName: String,
    val lastModified: Long
)
