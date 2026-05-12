// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.netplay.model

import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch
import org.dolphinemu.dolphinemu.features.netplay.NetplaySession
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting
import org.dolphinemu.dolphinemu.features.settings.model.NativeConfig

class NetplayViewModel(
    private val netplaySession: NetplaySession,
) : ViewModel() {

    val launchGame = netplaySession.launchGame

    val isHosting = netplaySession.isHosting

    val connectionLost = netplaySession.connectionLost

    val players = netplaySession.players
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(), emptyList())

    val messages = netplaySession.messages
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(), emptyList())

    val game = netplaySession.game
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(), "")

    val hostInputAuthority = netplaySession.hostInputAuthorityEnabled
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(), false)

    private val _maxBuffer = MutableStateFlow(IntSetting.NETPLAY_CLIENT_BUFFER_SIZE.int)
    val maxBuffer = _maxBuffer.asStateFlow()

    val saveTransferProgress = netplaySession.saveTransferProgress

    val gameDigestProgress = netplaySession.gameDigestProgress

    fun startGame() {
        netplaySession.startGame()
    }

    fun sendMessage(message: String) {
        val trimmedMessage = message.trim()
        if (trimmedMessage.isEmpty()) {
            return
        }

        netplaySession.sendMessage(trimmedMessage)
    }

    fun setMaxBuffer(buffer: Int) {
        _maxBuffer.value = buffer
        IntSetting.NETPLAY_CLIENT_BUFFER_SIZE.setInt(NativeConfig.LAYER_BASE, buffer)
        netplaySession.adjustPadBufferSize(buffer)
    }

    @OptIn(DelicateCoroutinesApi::class)
    override fun onCleared() {
        super.onCleared()
        // Closing the netplay session is a bit slow for the main thread so launch in
        // GlobalScope and allow the activity and view model to finish immediately.
        GlobalScope.launch {
            netplaySession.close()
        }
    }

    class Factory(private val session: NetplaySession) : ViewModelProvider.Factory {
        @Suppress("UNCHECKED_CAST")
        override fun <T : ViewModel> create(modelClass: Class<T>): T {
            return NetplayViewModel(session) as T
        }
    }
}
