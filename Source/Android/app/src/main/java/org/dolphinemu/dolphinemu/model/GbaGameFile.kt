// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.model

import org.dolphinemu.dolphinemu.features.dualscreen.GbaBootManager

data class GbaGameFile(val path: String, val rootPath: String) {
    val title = GbaBootManager.titleFor(path)
    val folderLabel: String

    init {
        val rootPrefix = rootPath.trimEnd('/')
        val relativePath = if (path.startsWith("$rootPrefix/")) {
            path.removePrefix("$rootPrefix/")
        } else {
            GbaBootManager.displayNameFor(path)
        }

        val relativeFolder = relativePath.substringBeforeLast('/', "")
        folderLabel = relativeFolder.ifEmpty { GbaBootManager.displayNameFor(rootPath) }
    }
}
