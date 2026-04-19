// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.netplay.model

import androidx.lifecycle.ViewModel
import androidx.lifecycle.asFlow
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.channels.Channel.Factory.CONFLATED
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.receiveAsFlow
import kotlinx.coroutines.launch
import org.dolphinemu.dolphinemu.features.netplay.Netplay
import org.dolphinemu.dolphinemu.services.GameFileCacheManager

class NetplaySetupViewModel : ViewModel() {
    private val _connectionRole = MutableStateFlow<ConnectionRole>(ConnectionRole.Connect)
    val connectionRole = _connectionRole.asStateFlow()

    private val _nickname = MutableStateFlow(Netplay.Settings.getNickname())
    val nickname = _nickname.asStateFlow()

    private val _connectionType = MutableStateFlow(Netplay.Settings.getConnectionType())
    val connectionType = _connectionType.asStateFlow()

    private val _ipAddress = MutableStateFlow(Netplay.Settings.getAddress())
    val ipAddress = _ipAddress.asStateFlow()

    private val _hostCode = MutableStateFlow(Netplay.Settings.getHostCode())
    val hostCode = _hostCode.asStateFlow()

    private val _connectPort = MutableStateFlow(Netplay.Settings.getConnectPort().toString())
    val connectPort = _connectPort.asStateFlow()

    private val _showNetplayScreen = Channel<Unit>(CONFLATED)
    val showNetplayScreen = _showNetplayScreen.receiveAsFlow()

    private val _connecting = MutableStateFlow(false)
    val connecting = _connecting.asStateFlow()

    val errors = Netplay.connectionErrors

    init {
        GameFileCacheManager.startLoad()
    }

    fun setConnectionRole(connectionRole: ConnectionRole) {
        _connectionRole.value = connectionRole
    }

    fun setNickname(nickname: String) {
        _nickname.value = nickname
        Netplay.Settings.setNickname(nickname)
    }

    fun setConnectionType(connectionType: ConnectionType) {
        _connectionType.value = connectionType
        Netplay.Settings.setTraversalChoice(connectionType.configValue)
    }

    fun setIpAddress(ipAddress: String) {
        if (ipAddress.all { it.isDigit() || it == '.' }) {
            _ipAddress.value = ipAddress
            Netplay.Settings.setAddress(ipAddress)
        }
    }

    fun setHostCode(hostCode: String) {
        _hostCode.value = hostCode
        Netplay.Settings.setHostCode(hostCode)
    }

    fun setConnectPort(port: String) {
        if (port.all { it.isDigit() }) {
            _connectPort.value = port
            port.toIntOrNull()?.let { Netplay.Settings.setConnectPort(it) }
        }
    }

    fun connect() {
        _connecting.value = true

        viewModelScope.launch {
            GameFileCacheManager.isLoading().asFlow().first { it == false }

            if (Netplay.join()) {
                _showNetplayScreen.trySend(Unit)
            }

            _connecting.value = false
        }
    }
}
