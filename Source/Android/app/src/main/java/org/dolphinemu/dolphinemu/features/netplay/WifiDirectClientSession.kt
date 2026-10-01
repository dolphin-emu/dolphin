package org.dolphinemu.dolphinemu.features.netplay

import android.annotation.SuppressLint
import android.app.Application
import android.net.wifi.p2p.WifiP2pConfig
import android.net.wifi.p2p.WifiP2pManager
import android.net.wifi.p2p.nsd.WifiP2pDnsSdServiceRequest
import android.os.SystemClock
import android.util.Log
import kotlinx.coroutines.NonCancellable
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.filter
import kotlinx.coroutines.flow.filterNotNull
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.merge
import kotlinx.coroutines.flow.onEach
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeoutOrNull
import kotlin.time.Duration.Companion.seconds

class WifiDirectClientSession(
    application: Application,
    manager: WifiP2pManager,
    onClosed: () -> Unit,
) : WifiDirectSession(application, manager, onClosed) {

    override val TAG: String = "WifiDirectClientSession"

    sealed interface ConnectResult {
        data class Success(val groupOwnerAddress: String) : ConnectResult

        data class Failure(val message: String) : ConnectResult
    }

    private data class LastSeenHost(
        val host: Host,
        val timestamp: Long,
    )

    private val _hosts = MutableStateFlow<Map<String, LastSeenHost>>(emptyMap())
    val hosts = combine(_hosts, peers) { hosts, peers ->
        hosts
            .map { it.value.host }
            .filter { host ->
                peers.any { it.deviceAddress == host.deviceAddress }
            }
    }

    init {
        val txtListener = WifiP2pManager.DnsSdTxtRecordListener { fullDomain, record, device ->
            if (fullDomain.contains(SERVICE_TYPE)) {
                _hosts.value = _hosts.value.toMutableMap().apply {
                    put(
                        key = device.deviceAddress,
                        value = LastSeenHost(
                            host = Host(
                                deviceAddress = device.deviceAddress,
                                name = record[TXT_MAP_NAME]!!,
                                playerCount = record[TXT_MAP_PLAYER_COUNT]!!,
                                game = record[TXT_MAP_GAME]!!,
                            ),
                            timestamp = SystemClock.elapsedRealtime(),
                        )
                    )
                }
            }
        }

        manager.setDnsSdResponseListeners(channel, null, txtListener)
    }

    override suspend fun onClose() {
        _hosts.value = emptyMap()
    }

    /**
     * Start discovering dolphin netplay services. Run until the coroutine is cancelled or an
     * error occurs.
     */
    @SuppressLint("MissingPermission")
    suspend fun runDiscovery(): Result.Failure {
        _hosts.value = emptyMap()

        val serviceRequest = WifiP2pDnsSdServiceRequest.newInstance()
        val addServiceRequestResult =
            awaitActionListener { manager.addServiceRequest(channel, serviceRequest, it) }
        if (addServiceRequestResult is ActionListenerResult.Failure) {
            return Result.Failure("addServiceRequest failed with reason=${addServiceRequestResult.reason}")
        }

        var consecutiveFailedAttempts = 0
        val maxAttempts = 3
        try {
            while (true) {
                val now = SystemClock.elapsedRealtime()
                _hosts.value = _hosts.value.filterValues {
                    now - it.timestamp < HOST_EXPIRY.inWholeMilliseconds
                }

                Log.d(TAG, "Discover services")
                val discoverServicesResult = awaitActionListener {
                    manager.discoverServices(channel, it)
                }
                when (discoverServicesResult) {
                    ActionListenerResult.Success -> consecutiveFailedAttempts = 0
                    is ActionListenerResult.Failure -> {
                        if (++consecutiveFailedAttempts > maxAttempts) {
                            return Result.Failure("discoverServices failed with reason=${discoverServicesResult.reason} after $maxAttempts attempts.")
                        }
                    }
                }
                delay(DISCOVERY_INTERVAL)
            }
        } finally {
            withContext(NonCancellable) {
                awaitActionListener { manager.clearServiceRequests(channel, it) }
            }
        }
    }

    @SuppressLint("NewApi", "MissingPermission")
    suspend fun connect(wifiDirectHost: Host): ConnectResult {
        Log.d("TAG", "connect()")

        val config = WifiP2pConfig.Builder()
            .setNetworkName(NETWORK_NAME)
            .setPassphrase(PASSPHRASE)
            .build()

        when (val connectResult = awaitActionListener { manager.connect(channel, config, it) }) {
            ActionListenerResult.Success -> Unit

            is ActionListenerResult.Failure -> {
                return ConnectResult.Failure("connect failed with reason=${connectResult.reason}")
            }
        }

        val success = currentHostAddress
            .onEach { Log.d(TAG, "currentHostAddress $it") }
            .filterNotNull()
            .map { ConnectResult.Success(it) }

        val failure = peers
            .filter { peers -> peers.none { it.deviceAddress == wifiDirectHost.deviceAddress } }
            .map { ConnectResult.Failure("connect failed since host disappeared from peers list") }

        return withTimeoutOrNull(CONNECT_TIMEOUT) {
            merge(success, failure).first()
        } ?: ConnectResult.Failure("connect failed after a ${CONNECT_TIMEOUT.inWholeSeconds} second timeout")
    }

    companion object {
        private val HOST_EXPIRY = 10.seconds

        private val DISCOVERY_INTERVAL = 3.seconds

        private val CONNECT_TIMEOUT = 120.seconds
    }
}
