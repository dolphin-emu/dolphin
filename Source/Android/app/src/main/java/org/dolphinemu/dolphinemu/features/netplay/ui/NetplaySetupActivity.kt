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
import androidx.lifecycle.compose.LifecycleStartEffect
import androidx.lifecycle.flowWithLifecycle
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.onEach
import org.dolphinemu.dolphinemu.features.netplay.NetplayManager
import org.dolphinemu.dolphinemu.features.netplay.WifiDirectManager
import org.dolphinemu.dolphinemu.features.netplay.model.NetplaySetupViewModel
import org.dolphinemu.dolphinemu.ui.main.ThemeProvider
import org.dolphinemu.dolphinemu.ui.theme.DolphinTheme
import org.dolphinemu.dolphinemu.utils.ThemeHelper

class NetplaySetupActivity : AppCompatActivity(), ThemeProvider {
    override var themeId: Int = 0

    override fun onCreate(savedInstanceState: Bundle?) {
        ThemeHelper.setTheme(this)
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)

        val viewModel = ViewModelProvider(
            this,
            NetplaySetupViewModel.Factory(NetplayManager, WifiDirectManager)
        )[NetplaySetupViewModel::class.java]

        viewModel.showNetplayScreen
            .flowWithLifecycle(lifecycle, Lifecycle.State.STARTED)
            .onEach { NetplayActivity.launch(this) }
            .launchIn(lifecycleScope)

        setContent {
            DolphinTheme {
                LifecycleStartEffect(Unit) {
                    viewModel.onScreenVisible()
                    onStopOrDispose {
                        if (!isChangingConfigurations) {
                            viewModel.onScreenHidden()
                        }
                    }
                }

                NetplaySetupScreen(
                    onBackClicked = { finish() },
                    connecting = viewModel.connecting.collectAsState().value,
                    errors = viewModel.errors,
                    connectionRole = viewModel.connectionRole.collectAsState().value,
                    onConnectionRoleChanged = viewModel::setConnectionRole,
                    nickname = viewModel.nickname.collectAsState().value,
                    onNicknameChanged = viewModel::setNickname,
                    supportedConnectionTypes = viewModel.supportedConnectionTypes,
                    connectionType = viewModel.connectionType.collectAsState().value,
                    onConnectionTypeChanged = viewModel::setConnectionType,
                    ipAddress = viewModel.ipAddress.collectAsState().value,
                    onIpAddressChanged = viewModel::setIpAddress,
                    connectPort = viewModel.connectPort.collectAsState().value,
                    onConnectPortChanged = viewModel::setConnectPort,
                    hostCode = viewModel.hostCode.collectAsState().value,
                    onHostCodeChanged = viewModel::setHostCode,
                    hostPort = viewModel.hostPort.collectAsState().value,
                    onHostPortChanged = viewModel::setHostPort,
                    useUpnp = viewModel.useUpnp.collectAsState().value,
                    onUseUpnpChanged = viewModel::setUseUpnp,
                    wifiDirectHosts = viewModel.wifiDirectHosts.collectAsState().value,
                    onWifiDirectHostClicked = viewModel::connect,
                    onHostClicked = viewModel::host,
                    onConnectClicked = viewModel::connect,
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
            context.startActivity(Intent(context, NetplaySetupActivity::class.java))
        }
    }
}
