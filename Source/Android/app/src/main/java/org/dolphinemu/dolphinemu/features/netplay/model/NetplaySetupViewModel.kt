// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.netplay.model

import androidx.lifecycle.ViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow

class NetplaySetupViewModel : ViewModel() {
    private val _connectionRole = MutableStateFlow<ConnectionRole>(ConnectionRole.Connect)
    val connectionRole = _connectionRole.asStateFlow()

    private val _nickname = MutableStateFlow("")
    val nickname = _nickname.asStateFlow()

    private val _connectionType = MutableStateFlow<ConnectionType>(ConnectionType.DirectConnection)
    val connectionType = _connectionType.asStateFlow()

    private val _ipAddress = MutableStateFlow("")
    val ipAddress = _ipAddress.asStateFlow()

    private val _hostCode = MutableStateFlow("")
    val hostCode = _hostCode.asStateFlow()

    private val _connectPort = MutableStateFlow(0.toString())
    val connectPort = _connectPort.asStateFlow()

    fun setConnectionRole(connectionRole: ConnectionRole) {
        _connectionRole.value = connectionRole
    }

    fun setNickname(nickname: String) {
        _nickname.value = nickname
    }

    fun setConnectionType(connectionType: ConnectionType) {
        _connectionType.value = connectionType
    }

    fun setIpAddress(ipAddress: String) {
        if (ipAddress.all { it.isDigit() || it == '.' }) {
            _ipAddress.value = ipAddress
        }
    }

    fun setHostCode(hostCode: String) {
        _hostCode.value = hostCode
    }

    fun setConnectPort(port: String) {
        if (port.all { it.isDigit() }) {
            _connectPort.value = port
        }
    }

    fun connect() {
    }
}
