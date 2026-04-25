package org.dolphinemu.dolphinemu.features.netplay.model

data class SaveTransferProgress(
    val title: String,
    val totalSize: Long,
    val playerProgresses: List<PlayerProgress>
) {
    data class PlayerProgress(
        val playerId: Int,
        val name: String,
        val progress: Long,
    )
}
