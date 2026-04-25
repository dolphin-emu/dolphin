// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.netplay.model

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.channels.Channel.Factory.CONFLATED
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.receiveAsFlow
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch
import org.dolphinemu.dolphinemu.features.netplay.Netplay

//TODO save settings
class NetplayViewModel : ViewModel() {
    val launchGame = Netplay.launchGame

    private val _goBack = Channel<Unit>(CONFLATED)
    val goBack = _goBack.receiveAsFlow()

    val connectionLost = Netplay.connectionLost

    val players = Netplay.players
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(), emptyList())

    val messages = Netplay.messages
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(), emptyList())

    val game = Netplay.game
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(), "")

    val hostInputAuthority = Netplay.hostInputAuthorityEnabled
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(), false)

    private val _maxBuffer = MutableStateFlow(Netplay.Settings.getClientBufferSize())
    val maxBuffer = _maxBuffer.asStateFlow()

    val saveTransferProgress = Netplay.saveTransferProgress

    init {
        if (!Netplay.isClientConnected()) {
            _goBack.trySend(Unit)
        }
    }

    fun sendMessage(message: String) {
        val trimmedMessage = message.trim()
        if (trimmedMessage.isEmpty()) {
            return
        }

        Netplay.sendMessage(trimmedMessage)
    }

    fun setMaxBuffer(buffer: Int) {
        _maxBuffer.value = buffer
        Netplay.Settings.setClientBufferSize(buffer)
        Netplay.adjustPadBufferSize(buffer)
    }

    @OptIn(DelicateCoroutinesApi::class)
    override fun onCleared() {
        super.onCleared()
        GlobalScope.launch {
            Netplay.quit()
        }
    }
}
