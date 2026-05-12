// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.netplay.model

import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.asFlow
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.channels.Channel.Factory.CONFLATED
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.onEach
import kotlinx.coroutines.flow.receiveAsFlow
import kotlinx.coroutines.launch
import org.dolphinemu.dolphinemu.features.netplay.NetplayManager
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting
import org.dolphinemu.dolphinemu.features.settings.model.NativeConfig
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting
import org.dolphinemu.dolphinemu.services.GameFileCacheManager

class NetplaySetupViewModel(
    private val netplayManager: NetplayManager,
) : ViewModel() {

    private val _connectionRole = MutableStateFlow<ConnectionRole>(ConnectionRole.Connect)
    val connectionRole = _connectionRole.asStateFlow()

    private val _nickname = MutableStateFlow(StringSetting.NETPLAY_NICKNAME.string)
    val nickname = _nickname.asStateFlow()

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

    private val _showNetplayScreen = Channel<Unit>(CONFLATED)
    val showNetplayScreen = _showNetplayScreen.receiveAsFlow()

    private val _connecting = MutableStateFlow(false)
    val connecting = _connecting.asStateFlow()

    private val _errors = MutableSharedFlow<String>(extraBufferCapacity = 8)
    val errors = _errors.asSharedFlow()

    init {
        GameFileCacheManager.startLoad()
    }

    fun setConnectionRole(connectionRole: ConnectionRole) {
        _connectionRole.value = connectionRole
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

    fun connect() {
        if (_connecting.value) return

        _connecting.value = true

        viewModelScope.launch {
            var errorForwarding: Job? = null

            try {
                GameFileCacheManager.isLoading().asFlow().first { it == false }

                val session = netplayManager.createSession()
                errorForwarding = session.connectionErrors
                    .onEach { _errors.emit(it) }
                    .launchIn(this)

                if (session.join()) {
                    _showNetplayScreen.trySend(Unit)
                }
            } finally {
                errorForwarding?.cancel()
                _connecting.value = false
            }
        }
    }

    override fun onCleared() {
        super.onCleared()
        // There should not be an active session at this point but in case one was created
        // but launching the Netplay screen failed, close it.
        netplayManager.activeSession?.closeBlocking()
    }

    class Factory(private val netplayManager: NetplayManager) : ViewModelProvider.Factory {
        @Suppress("UNCHECKED_CAST")
        override fun <T : ViewModel> create(modelClass: Class<T>): T {
            return NetplaySetupViewModel(netplayManager) as T
        }
    }
}
