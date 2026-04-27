package org.dolphinemu.dolphinemu.features.netplay.model

import androidx.annotation.StringRes
import org.dolphinemu.dolphinemu.R

sealed class ConnectionType(
    @StringRes val labelId: Int,
    val configValue: String,
) {
    object DirectConnection : ConnectionType(
        labelId = R.string.netplay_connection_type_direct_connection,
        configValue = "direct",
    )

    object TraversalServer : ConnectionType(
        labelId = R.string.netplay_connection_type_traversal_server,
        configValue = "traversal",
    )

    companion object {
        val all: List<ConnectionType>
            get() = listOf(DirectConnection, TraversalServer)

        fun fromString(value: String): ConnectionType =
            all.find { it.configValue == value } ?: throw IllegalArgumentException("Invalid connection type: $value")
    }
}
