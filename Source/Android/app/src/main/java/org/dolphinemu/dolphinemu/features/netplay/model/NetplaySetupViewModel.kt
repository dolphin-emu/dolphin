// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.netplay.model

import android.util.Log
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.asFlow
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.cancelAndJoin
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.channels.Channel.Factory.CONFLATED
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.emptyFlow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.flatMapLatest
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.onEach
import kotlinx.coroutines.flow.receiveAsFlow
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch
import org.dolphinemu.dolphinemu.features.netplay.NetplayManager
import org.dolphinemu.dolphinemu.features.netplay.WifiDirectClientSession
import org.dolphinemu.dolphinemu.features.netplay.WifiDirectManager
import org.dolphinemu.dolphinemu.features.netplay.WifiDirectSession
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting
import org.dolphinemu.dolphinemu.features.settings.model.NativeConfig
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting
import org.dolphinemu.dolphinemu.services.GameFileCacheManager

class NetplaySetupViewModel(
    private val netplayManager: NetplayManager,
    private val wifiDirectManager: WifiDirectManager = WifiDirectManager,
) : ViewModel() {

    private val _connectionRole = MutableStateFlow<ConnectionRole>(ConnectionRole.Connect)
    val connectionRole = _connectionRole.asStateFlow()

    private val _nickname = MutableStateFlow(StringSetting.NETPLAY_NICKNAME.string)
    val nickname = _nickname.asStateFlow()

    val supportedConnectionTypes = buildList {
        add(ConnectionType.DirectConnection)
        add(ConnectionType.TraversalServer)
        if (wifiDirectManager.isWifiDirectSupported) {
            add(ConnectionType.WifiDirect)
        }
    }

    private val _connectionType = MutableStateFlow(
        ConnectionType.fromString(StringSetting.NETPLAY_TRAVERSAL_CHOICE.string)
    )
    val connectionType = _connectionType.asStateFlow()

    private val _ipAddress = MutableStateFlow(StringSetting.NETPLAY_ADDRESS.string)
    val ipAddress = _ipAddress.asStateFlow()

    private val _hostCode = MutableStateFlow(StringSetting.NETPLAY_HOST_CODE.string)
    val hostCode = _hostCode.asStateFlow()

    private val _connectPort = MutableStateFlow(IntSetting.NETPLAY_CONNECT_PORT.int.toString())
    val connectPort = _connectPort.asStateFlow()

    private val _hostPort = MutableStateFlow(IntSetting.NETPLAY_HOST_PORT.int.toString())
    val hostPort = _hostPort.asStateFlow()

    private val _useUpnp = MutableStateFlow(BooleanSetting.NETPLAY_USE_UPNP.boolean)
    val useUpnp = _useUpnp.asStateFlow()

    private val _showNetplayScreen = Channel<Unit>(CONFLATED)
    val showNetplayScreen = _showNetplayScreen.receiveAsFlow()

    private val _connecting = MutableStateFlow(false)
    val connecting = _connecting.asStateFlow()

    private val _errors = MutableSharedFlow<NetplaySetupError>(extraBufferCapacity = 8)
    val errors = _errors.asSharedFlow()

    private val wifiDirectClientSession: WifiDirectClientSession?
        get() = wifiDirectManager.activeSession as? WifiDirectClientSession

    private val isInWifiDirectClientMode
        get() = connectionRole.value == ConnectionRole.Connect && connectionType.value == ConnectionType.WifiDirect

    private var wifiDirectDiscoveryJob: Job? = null

    private var wifiDirectConnectingJob: Job? = null

    private val _wifiDirectHostsSource =
        MutableStateFlow(emptyFlow<List<WifiDirectSession.Host>>())

    @OptIn(ExperimentalCoroutinesApi::class)
    val wifiDirectHosts = _wifiDirectHostsSource
        .flatMapLatest { it }
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(), emptyList())

    init {
        GameFileCacheManager.startLoad()
    }

    fun setConnectionRole(connectionRole: ConnectionRole) {
        _connectionRole.value = connectionRole
        startOrStopWifiDirectAsClient()
    }

    fun setNickname(nickname: String) {
        _nickname.value = nickname
        StringSetting.NETPLAY_NICKNAME.setString(NativeConfig.LAYER_BASE, nickname)
    }

    fun setConnectionType(connectionType: ConnectionType) {
        _connectionType.value = connectionType
        StringSetting.NETPLAY_TRAVERSAL_CHOICE.setString(
            NativeConfig.LAYER_BASE, connectionType.configValue
        )
        startOrStopWifiDirectAsClient()
    }

    fun setIpAddress(ipAddress: String) {
        if (ipAddress.all { it.isDigit() || it == '.' }) {
            _ipAddress.value = ipAddress
            StringSetting.NETPLAY_ADDRESS.setString(NativeConfig.LAYER_BASE, ipAddress)
        }
    }

    fun setHostCode(hostCode: String) {
        _hostCode.value = hostCode
        StringSetting.NETPLAY_HOST_CODE.setString(NativeConfig.LAYER_BASE, hostCode)
    }

    fun setConnectPort(port: String) {
        if (port.all { it.isDigit() }) {
            _connectPort.value = port
            port.toIntOrNull()?.let {
                IntSetting.NETPLAY_CONNECT_PORT.setInt(NativeConfig.LAYER_BASE, it)
            }
        }
    }

    fun setHostPort(port: String) {
        if (port.all { it.isDigit() }) {
            _hostPort.value = port
            port.toIntOrNull()?.let {
                IntSetting.NETPLAY_HOST_PORT.setInt(NativeConfig.LAYER_BASE, it)
            }
        }
    }

    fun setUseUpnp(useUpnp: Boolean) {
        _useUpnp.value = useUpnp
        BooleanSetting.NETPLAY_USE_UPNP.setBoolean(NativeConfig.LAYER_BASE, useUpnp)
    }

    fun host() = connect(host = true, wifiDirectHost = null)

    fun connect() = connect(host = false, wifiDirectHost = null)

    fun connect(wifiDirectHost: WifiDirectSession.Host) =
        connect(host = false, wifiDirectHost = wifiDirectHost)

    fun onScreenVisible() {
        startWifiDirectDiscovery()
    }

    fun onScreenHidden() {
        viewModelScope.launch {
            stopWifiDirectDiscovery()
        }
    }

    private fun connect(
        host: Boolean,
        wifiDirectHost: WifiDirectSession.Host?,
    ) {
        if (wifiDirectConnectingJob?.isActive == true) return
        if (_connecting.value) return
        _connecting.value = true

        wifiDirectConnectingJob = viewModelScope.launch {
            var errorForwarding: Job? = null

            try {
                if (_connectionType.value == ConnectionType.WifiDirect) {
                    if (host) {
                        val wifiDirectHostSession = wifiDirectManager.createHostSession()
                        val result = wifiDirectHostSession.createGroup()
                        if (result is WifiDirectSession.Result.Failure) {
                            Log.d(TAG, "createHostSession failed with message: ${result.message}")
                            _errors.emit(NetplaySetupError.WifiDirectHost)
                            wifiDirectHostSession.close()
                            return@launch
                        }
                    } else if (wifiDirectHost != null) {
                        val wifiDirectClientSession =
                            wifiDirectManager.activeSession as? WifiDirectClientSession
                                ?: return@launch

                        stopWifiDirectDiscovery()

                        when (val result = wifiDirectClientSession.connect(wifiDirectHost)) {
                            is WifiDirectClientSession.ConnectResult.Success -> {
                                StringSetting.NETPLAY_ADDRESS.setString(
                                    NativeConfig.LAYER_BASE, result.groupOwnerAddress
                                )
                            }

                            is WifiDirectClientSession.ConnectResult.Failure -> {
                                Log.d(TAG, "connect failed with message: ${result.message}")
                                _errors.emit(NetplaySetupError.WifiDirectConnect)
                                startWifiDirectDiscovery()
                                return@launch
                            }
                        }
                    }
                }

                GameFileCacheManager.isLoading().asFlow().first { it == false }

                val session = netplayManager.createSession()
                errorForwarding = session.connectionErrors
                    .onEach { _errors.emit(NetplaySetupError.FromString(it)) }
                    .launchIn(this)

                val success = if (host) {
                    session.host()
                } else {
                    session.join()
                }
                if (success) {
                    _showNetplayScreen.trySend(Unit)
                } else {
                    // Reset wifi direct state in the event that wifi direct connects but netplay fails.
                    if (isInWifiDirectClientMode) {
                        wifiDirectClientSession?.clearGroupAndPeers()
                        startWifiDirectDiscovery()
                    }
                }
            } finally {
                errorForwarding?.cancel()
                _connecting.value = false
            }
        }
    }

    private fun startOrStopWifiDirectAsClient() {
        if (isInWifiDirectClientMode) {
            viewModelScope.launch {
                wifiDirectManager.createClientSession()
                startWifiDirectDiscovery()
            }
        } else {
            viewModelScope.launch {
                stopConnecting()
                stopWifiDirectDiscovery()
                wifiDirectManager.activeSession?.close()
            }
        }
    }

    private fun startWifiDirectDiscovery() {
        if (wifiDirectDiscoveryJob?.isActive == true) return
        if (!isInWifiDirectClientMode) return
        val session = wifiDirectClientSession ?: return

        _wifiDirectHostsSource.value = session.hosts

        wifiDirectDiscoveryJob = viewModelScope.launch {
            val failure = session.runDiscovery()
            Log.d(TAG, "runDiscovery() failed with message: ${failure.message}")
            _errors.emit(NetplaySetupError.WifiDirectDiscovery)
            setConnectionType(ConnectionType.DirectConnection)
        }
    }

    private suspend fun stopWifiDirectDiscovery() {
        val job = wifiDirectDiscoveryJob ?: return
        wifiDirectDiscoveryJob = null
        job.cancelAndJoin()
    }

    private suspend fun stopConnecting() {
        val job = wifiDirectConnectingJob ?: return
        wifiDirectConnectingJob = null
        job.cancelAndJoin()
    }

    override fun onCleared() {
        // There should not be an active netplay session at this point but in case one was created
        // but launching the Netplay screen failed, close it.
        GlobalScope.launch {
            netplayManager.activeSession?.close()
            wifiDirectManager.activeSession?.close()
        }
    }

    class Factory(
        private val netplayManager: NetplayManager,
        private val wifiDirectManager: WifiDirectManager,
    ) : ViewModelProvider.Factory {
        @Suppress("UNCHECKED_CAST")
        override fun <T : ViewModel> create(modelClass: Class<T>): T {
            return NetplaySetupViewModel(netplayManager, wifiDirectManager) as T
        }
    }

    private companion object {
        private const val TAG = "NetplaySetupViewModel"
    }
}
