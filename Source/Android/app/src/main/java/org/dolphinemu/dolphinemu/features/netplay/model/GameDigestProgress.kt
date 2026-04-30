package org.dolphinemu.dolphinemu.features.netplay.model

data class GameDigestProgress(
    val title: String,
    val playerProgresses: List<PlayerProgress>,
    val matches: Boolean?,
) {
    data class PlayerProgress(
        val playerId: Int,
        val name: String,
        val progress: Int,
        val result: String?,
    )
}
