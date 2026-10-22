// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.netplay.model

data class GameDigestProgress(
    val title: String,
    val playerProgresses: List<PlayerProgress>,
    val finished: Boolean,
) {
    val matches: Boolean?
        get() {
            val results = playerProgresses.mapNotNull { it.result }
            return if (results.size >= 2) results.distinct().size == 1 else null
        }

    data class PlayerProgress(
        val playerId: Int,
        val name: String,
        val progress: Int,
        val result: String?,
    )
}
