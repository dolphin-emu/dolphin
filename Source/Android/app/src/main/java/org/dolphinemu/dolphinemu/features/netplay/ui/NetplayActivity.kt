// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.netplay.ui

import android.content.Context
import android.content.Intent
import android.os.Bundle
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.compose.runtime.collectAsState
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.flowWithLifecycle
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.onEach
import org.dolphinemu.dolphinemu.activities.EmulationActivity
import org.dolphinemu.dolphinemu.features.netplay.NetplayManager
import org.dolphinemu.dolphinemu.features.netplay.model.NetplayViewModel
import org.dolphinemu.dolphinemu.ui.main.ThemeProvider
import org.dolphinemu.dolphinemu.ui.theme.DolphinTheme
import org.dolphinemu.dolphinemu.utils.ThemeHelper

class NetplayActivity : AppCompatActivity(), ThemeProvider {
    override var themeId: Int = 0

    override fun onCreate(savedInstanceState: Bundle?) {
        ThemeHelper.setTheme(this)
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)

        val session = NetplayManager.activeSession
        if (session == null) {
            finish()
            return
        }

        val viewModel = ViewModelProvider(this, NetplayViewModel.Factory(session))[NetplayViewModel::class.java]

        viewModel.launchGame
            .flowWithLifecycle(lifecycle, Lifecycle.State.STARTED)
            .onEach { EmulationActivity.launch(this, it, false) }
            .launchIn(lifecycleScope)

        setContent {
            DolphinTheme {
                NetplayScreen(
                    onBackClicked = { finish() },
                    connectionLost = viewModel.connectionLost,
                    messages = viewModel.messages.collectAsState().value,
                    onSendMessage = viewModel::sendMessage,
                    game = viewModel.game.collectAsState().value,
                    players = viewModel.players.collectAsState().value,
                    hostInputAuthorityEnabled = viewModel.hostInputAuthority.collectAsState().value,
                    maxBuffer = viewModel.maxBuffer.collectAsState().value,
                    onMaxBufferChanged = viewModel::setMaxBuffer,
                    saveTransferProgress = viewModel.saveTransferProgress.collectAsState().value,
                    gameDigestProgress = viewModel.gameDigestProgress.collectAsState().value,
                )
            }
        }
    }

    override fun setTheme(themeId: Int) {
        super.setTheme(themeId)
        this.themeId = themeId
    }

    override fun onResume() {
        ThemeHelper.setCorrectTheme(this)
        super.onResume()
    }

    companion object {
        @JvmStatic
        fun launch(context: Context) {
            context.startActivity(Intent(context, NetplayActivity::class.java))
        }
    }
}
