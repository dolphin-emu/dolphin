// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.netplay.model

import androidx.lifecycle.ViewModel
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.channels.Channel.Factory.CONFLATED
import kotlinx.coroutines.flow.receiveAsFlow
import kotlinx.coroutines.launch
import org.dolphinemu.dolphinemu.features.netplay.Netplay

class NetplayViewModel : ViewModel() {
    val launchGame = Netplay.launchGame

    private val _goBack = Channel<Unit>(CONFLATED)
    val goBack = _goBack.receiveAsFlow()

    init {
        if (!Netplay.isClientConnected()) {
            _goBack.trySend(Unit)
        }
    }

    @OptIn(DelicateCoroutinesApi::class)
    override fun onCleared() {
        super.onCleared()
        GlobalScope.launch {
            Netplay.quit()
        }
    }
}
