package org.dolphinemu.dolphinemu.features.netplay

import android.annotation.SuppressLint
import android.app.Application
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.net.wifi.p2p.WifiP2pDevice
import android.net.wifi.p2p.WifiP2pDeviceList
import android.net.wifi.p2p.WifiP2pGroup
import android.net.wifi.p2p.WifiP2pInfo
import android.net.wifi.p2p.WifiP2pManager
import androidx.core.content.IntentCompat
import kotlinx.coroutines.NonCancellable
import kotlinx.coroutines.channels.BufferOverflow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeoutOrNull
import kotlin.coroutines.resume
import kotlin.time.Duration.Companion.seconds

abstract class WifiDirectSession(
    protected val application: Application,
    protected val manager: WifiP2pManager,
    private val onClosed: () -> Unit,
) : BroadcastReceiver() {

    protected abstract val TAG: String

    sealed interface Result {
        data object Success : Result
        data class Failure(val message: String) : Result
    }

    data class Host(
        val deviceAddress: String,
        val name: String,
        val playerCount: String,
        val game: String,
    )

    protected val channel: WifiP2pManager.Channel =
        manager.initialize(application, application.mainLooper, null)

    private val _currentGroup =
        MutableSharedFlow<WifiP2pGroup?>(replay = 1, onBufferOverflow = BufferOverflow.DROP_OLDEST)

    private val _currentWifiP2pInfo =
        MutableSharedFlow<WifiP2pInfo?>(replay = 1, onBufferOverflow = BufferOverflow.DROP_OLDEST)

    private val _peers = MutableStateFlow<Collection<WifiP2pDevice>>(emptyList())

    protected val currentGroupNetworkName = _currentGroup
        .map { it?.networkName }
        .distinctUntilChanged()

    protected val currentHostAddress = _currentWifiP2pInfo
        .map { it?.groupOwnerAddress?.hostAddress }
        .distinctUntilChanged()

    protected val peers = _peers.asStateFlow()

    val isGroupActive = currentGroupNetworkName
        .map { it != null }

    init {
        val intentFilter = IntentFilter().apply {
            addAction(WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION)
            addAction(WifiP2pManager.WIFI_P2P_PEERS_CHANGED_ACTION)
            addAction(WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION)
            addAction(WifiP2pManager.WIFI_P2P_THIS_DEVICE_CHANGED_ACTION)
            addAction(WifiP2pManager.WIFI_P2P_DISCOVERY_CHANGED_ACTION)
        }
        application.registerReceiver(this, intentFilter)
    }

    @Volatile
    var isClosed = false
        private set

    protected abstract suspend fun onClose()

    @SuppressLint("NewApi")
    suspend fun close() = withContext(NonCancellable) {
        if (isClosed) return@withContext
        isClosed = true

        onClose()
        clearGroupAndPeers()
        channel.close()
        application.unregisterReceiver(this@WifiDirectSession)
        onClosed()
    }

    suspend fun clearGroupAndPeers() {
        awaitActionListener { manager.removeGroup(channel, it) }
        withTimeoutOrNull(5.seconds) { _currentGroup.first { it == null } }
        awaitActionListener { manager.stopPeerDiscovery(channel, it) }
    }

    override fun onReceive(context: Context, intent: Intent) {
        when (intent.action) {
            WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION -> {
                val group = IntentCompat.getParcelableExtra(
                    intent,
                    WifiP2pManager.EXTRA_WIFI_P2P_GROUP,
                    WifiP2pGroup::class.java,
                )
                _currentGroup.tryEmit(group)

                val wifiP2pInfo = IntentCompat.getParcelableExtra(
                    intent,
                    WifiP2pManager.EXTRA_WIFI_P2P_INFO,
                    WifiP2pInfo::class.java,
                )
                _currentWifiP2pInfo.tryEmit(wifiP2pInfo)
            }

            WifiP2pManager.WIFI_P2P_PEERS_CHANGED_ACTION -> {
                val peers = IntentCompat.getParcelableExtra(
                    intent,
                    WifiP2pManager.EXTRA_P2P_DEVICE_LIST,
                    WifiP2pDeviceList::class.java,
                )
                _peers.value = peers?.deviceList.orEmpty()
            }
        }
    }

    protected sealed interface ActionListenerResult {
        data object Success : ActionListenerResult
        data class Failure(val reason: Int) : ActionListenerResult
    }

    protected suspend fun awaitActionListener(block: (WifiP2pManager.ActionListener) -> Unit): ActionListenerResult =
        suspendCancellableCoroutine { continuation ->
            block(
                object : WifiP2pManager.ActionListener {
                    override fun onSuccess() {
                        continuation.resume(ActionListenerResult.Success)
                    }

                    override fun onFailure(reason: Int) {
                        continuation.resume(ActionListenerResult.Failure(reason))
                    }
                }
            )
        }

    companion object {
        protected const val NETWORK_NAME = "DIRECT-aa-dolphin-netplay"

        protected const val PASSPHRASE = "dolphinnetplay"

        protected const val SERVICE_TYPE = "_dolphinnetplay._tcp"

        protected const val TXT_MAP_NAME = "name"

        protected const val TXT_MAP_PLAYER_COUNT = "player_count"

        protected const val TXT_MAP_GAME = "game"
    }
}
