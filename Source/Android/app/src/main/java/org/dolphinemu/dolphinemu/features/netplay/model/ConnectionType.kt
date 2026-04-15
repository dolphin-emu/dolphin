package org.dolphinemu.dolphinemu.features.netplay.model

import androidx.annotation.StringRes
import org.dolphinemu.dolphinemu.R

sealed class ConnectionType(
    @StringRes val labelId: Int,
) {
    object DirectConnection : ConnectionType(
        labelId = R.string.netplay_connection_type_direct_connection,
    )

    object TraversalServer : ConnectionType(
        labelId = R.string.netplay_connection_type_traversal_server,
    )

    companion object {
        val all: List<ConnectionType>
            get() = listOf(DirectConnection, TraversalServer)
    }
}
