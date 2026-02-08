// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.content.Context
import android.net.ConnectivityManager
import android.net.LinkAddress
import android.os.Build
import androidx.annotation.Keep
import androidx.annotation.RequiresApi
import org.dolphinemu.dolphinemu.DolphinApplication
import java.net.Inet4Address
import java.net.InetAddress
import java.net.UnknownHostException

@Keep
object NetworkHelper {
    private fun getConnectivityManager(): ConnectivityManager? {
        val context = DolphinApplication.getAppContext()
        val manager = context.getSystemService(Context.CONNECTIVITY_SERVICE) as? ConnectivityManager
        if (manager == null) {
            Log.warning("Cannot get network link as ConnectivityManager is null.")
        }
        return manager
    }

    @RequiresApi(Build.VERSION_CODES.M)
    private fun getIPv4Link(): LinkAddress? {
        val manager = getConnectivityManager() ?: return null
        val activeNetwork = manager.activeNetwork
        if (activeNetwork == null) {
            Log.warning("Active network is null.")
            return null
        }
        val properties = manager.getLinkProperties(activeNetwork)
        if (properties == null) {
            Log.warning("Link properties is null.")
            return null
        }
        return properties.linkAddresses.firstOrNull { it.address is Inet4Address } ?: run {
            Log.warning("No IPv4 link found.")
            null
        }
    }

    private fun InetAddress.inetAddressToInt(): Int {
        return address.foldIndexed(0) { index, acc, byte ->
            acc or ((byte.toInt() and 0xFF) shl (8 * index))
        }
    }

    @Keep
    @JvmStatic
    fun getNetworkIpAddress(): Int {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return 0
        return getIPv4Link()?.address?.inetAddressToInt() ?: 0
    }

    @Keep
    @JvmStatic
    fun getNetworkPrefixLength(): Int {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return 0
        return getIPv4Link()?.prefixLength ?: 0
    }

    @Keep
    @JvmStatic
    fun getNetworkGateway(): Int {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return 0
        val manager = getConnectivityManager() ?: return 0
        val activeNetwork = manager.activeNetwork ?: return 0
        val properties = manager.getLinkProperties(activeNetwork) ?: return 0
        val gatewayAddress = try {
            val target = InetAddress.getByName("8.8.8.8")
            properties.routes.firstOrNull { it.matches(target) }?.gateway
        } catch (ignored: UnknownHostException) {
            null
        }
        return gatewayAddress?.inetAddressToInt() ?: run {
            Log.warning("No valid gateway found.")
            0
        }
    }
}
