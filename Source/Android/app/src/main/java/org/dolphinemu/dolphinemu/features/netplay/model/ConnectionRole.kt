package org.dolphinemu.dolphinemu.features.netplay.model

import androidx.annotation.StringRes
import org.dolphinemu.dolphinemu.R

sealed class ConnectionRole(
    @StringRes val labelId: Int,
) {
    object Connect : ConnectionRole(R.string.netplay_connection_role_connect)
    object Host : ConnectionRole(R.string.netplay_connection_role_host)

    companion object {
        val all: List<ConnectionRole>
            get() = listOf(Connect, Host)
    }
}
