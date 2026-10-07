package org.dolphinemu.dolphinemu.features.netplay.model

import android.content.Context
import org.dolphinemu.dolphinemu.R

sealed class NetplaySetupError {
    abstract fun message(context: Context): String

    class FromString(private val message: String) : NetplaySetupError() {
        override fun message(context: Context) = message
    }

    object WifiDirectDiscovery : NetplaySetupError() {
        override fun message(context: Context) =
            context.getString(R.string.netplay_wifi_direct_discovery_failure)
    }

    object WifiDirectConnect : NetplaySetupError() {
        override fun message(context: Context) =
            context.getString(R.string.netplay_wifi_direct_connect_failure)
    }

    object WifiDirectHost : NetplaySetupError() {
        override fun message(context: Context) =
            context.getString(R.string.netplay_wifi_direct_host_failure)
    }
}
