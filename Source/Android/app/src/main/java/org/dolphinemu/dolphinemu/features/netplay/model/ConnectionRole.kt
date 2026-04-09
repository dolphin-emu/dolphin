package org.dolphinemu.dolphinemu.features.netplay.model

import androidx.annotation.StringRes
import org.dolphinemu.dolphinemu.R

sealed class ConnectionRole(
    @StringRes val labelId: Int,
    @StringRes val loadingLabelId: Int,
) {
    object Connect : ConnectionRole(
        labelId = R.string.netplay_connection_role_connect,
        loadingLabelId = R.string.netplay_connection_role_connect_loading,
    )

    object Host : ConnectionRole(
        labelId = R.string.netplay_connection_role_host,
        loadingLabelId = R.string.netplay_connection_role_host_loading,
    )

    companion object {
        val all: List<ConnectionRole>
            get() = listOf(Connect, Host)
    }
}
