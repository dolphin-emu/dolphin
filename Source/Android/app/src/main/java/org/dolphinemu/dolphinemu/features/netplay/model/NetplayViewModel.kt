// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.netplay.model

import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.asFlow
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch
import org.dolphinemu.dolphinemu.features.netplay.NetplaySession
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting
import org.dolphinemu.dolphinemu.features.settings.model.NativeConfig
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting
import org.dolphinemu.dolphinemu.model.GameFile
import org.dolphinemu.dolphinemu.services.GameFileCacheManager
import org.dolphinemu.dolphinemu.utils.NetworkHelper

class NetplayViewModel(
    private val netplaySession: NetplaySession,
    private val networkHelper: NetworkHelper,
) : ViewModel() {

    val launchGame = netplaySession.launchGame

    val isHosting = netplaySession.isHosting

    private val _joinAddresses = MutableStateFlow(
        mapOf(
            JoinInfoType.EXTERNAL to JoinAddress.Loading,
            JoinInfoType.LOCAL to getLocalIp(),
        )
    )
    val joinAddresses = _joinAddresses.asStateFlow()

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

    val gameFiles = GameFileCacheManager.getGameFiles().asFlow()
        .map { it.toList() }
        .stateIn(
            viewModelScope,
            SharingStarted.WhileSubscribed(),
            GameFileCacheManager.getGameFiles().value?.toList() ?: emptyList()
        )

    val saveTransferProgress = netplaySession.saveTransferProgress

    val gameDigestProgress = netplaySession.gameDigestProgress

    init {
        if (netplaySession.isHosting) {
            setInitialGame()
            fetchExternalIp()
        }
    }

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

    fun changeGame(gameFile: GameFile) {
        StringSetting.NETPLAY_GAME.setString(NativeConfig.LAYER_BASE, gameFile.getGameId())
        netplaySession.changeGame(gameFile)
    }

    private fun getLocalIp(): JoinAddress {
        val localIp = networkHelper.getLocalIpString()
            ?: return JoinAddress.Unknown { _joinAddresses.value += JoinInfoType.LOCAL to getLocalIp() }
        val port = netplaySession.getPort()
        return JoinAddress.Loaded("$localIp:$port")
    }

    private fun fetchExternalIp() {
        _joinAddresses.value += JoinInfoType.EXTERNAL to JoinAddress.Loading
        viewModelScope.launch(Dispatchers.IO) {
            val ip = netplaySession.getExternalIpAddress()
            val port = netplaySession.getPort()
            val address = if (ip != null) JoinAddress.Loaded("$ip:$port")
            else JoinAddress.Unknown { fetchExternalIp() }
            _joinAddresses.value += JoinInfoType.EXTERNAL to address
        }
    }

    private fun setInitialGame() {
        val game = gameFiles.value
            .find { it.getGameId() == StringSetting.NETPLAY_GAME.string }
            ?: gameFiles.value.firstOrNull()

        if (game != null) {
            changeGame(game)
        }
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

    class Factory(
        private val session: NetplaySession,
        private val networkHelper: NetworkHelper,
    ) : ViewModelProvider.Factory {
        @Suppress("UNCHECKED_CAST")
        override fun <T : ViewModel> create(modelClass: Class<T>): T {
            return NetplayViewModel(session, networkHelper) as T
        }
    }
}
