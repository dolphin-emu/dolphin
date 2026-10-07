package org.dolphinemu.dolphinemu.features.netplay

import android.app.Application
import android.net.wifi.p2p.WifiP2pConfig
import android.net.wifi.p2p.WifiP2pManager
import android.net.wifi.p2p.nsd.WifiP2pDnsSdServiceInfo
import android.util.Log
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.withTimeoutOrNull
import kotlin.time.Duration.Companion.seconds

class WifiDirectHostSession(
    application: Application,
    manager: WifiP2pManager,
    onClosed: () -> Unit,
) : WifiDirectSession(application, manager, onClosed) {

    override val TAG: String = "WifiDirectHostSession"

    override fun onClose() = Unit

    suspend fun setServiceInfo(hostName: String): Result {
        val clearLocalServicesResult = awaitActionListener { manager.clearLocalServices(channel, it) }
        if (clearLocalServicesResult is ActionListenerResult.Failure) {
            return Result.Failure("clearLocalServices failed with reason=${clearLocalServicesResult.reason}")
        }

        val serviceInfo = WifiP2pDnsSdServiceInfo.newInstance(
            "DolphinNetplay",
            SERVICE_TYPE,
            mapOf(
                TXT_MAP_NAME to hostName,
            ),
        )
        val addLocalServiceResult =
            awaitActionListener { manager.addLocalService(channel, serviceInfo, it) }
        if (addLocalServiceResult is ActionListenerResult.Failure) {
            return Result.Failure("addLocalService failed with reason=${addLocalServiceResult.reason}")
        }

        return Result.Success
    }

    suspend fun createGroup(): Result {
        val configBuilder = WifiP2pConfig.Builder()
            .setNetworkName(NETWORK_NAME)
            .setPassphrase(PASSPHRASE)

        val config = configBuilder.build()
        val createGroupResult = awaitActionListener { manager.createGroup(channel, config, it) }
        if (createGroupResult is ActionListenerResult.Failure) {
            return Result.Failure("createGroup failed with reason=${createGroupResult.reason}")
        } else {
            Log.d(TAG, "createGroup succeeded")
        }

        val expectedGroup = withTimeoutOrNull(GROUP_FORMATION_TIMEOUT) {
            currentGroupNetworkName.first { it == NETWORK_NAME }
        }
        if (expectedGroup == null) {
            return Result.Failure("Group did not form within $GROUP_FORMATION_TIMEOUT")
        }

        return Result.Success
    }

    companion object {
        private val GROUP_FORMATION_TIMEOUT = 15.seconds
    }
}
