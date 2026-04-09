// SPDX-License-Identifier: GPL-2.0-or-later

@file:OptIn(ExperimentalMaterial3Api::class)

package org.dolphinemu.dolphinemu.features.netplay.ui

import android.content.Context
import android.content.Intent
import android.os.Bundle
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.consumeWindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.ExposedDropdownMenuDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExtendedFloatingActionButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.MediumTopAppBar
import androidx.compose.material3.MenuAnchorType
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SecondaryTabRow
import androidx.compose.material3.Tab
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.emptyFlow
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.onEach
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.activities.EmulationActivity
import org.dolphinemu.dolphinemu.features.netplay.Netplay
import org.dolphinemu.dolphinemu.features.netplay.model.ConnectionRole
import org.dolphinemu.dolphinemu.features.netplay.model.ConnectionType
import org.dolphinemu.dolphinemu.features.netplay.model.NetplaySetupViewModel
import org.dolphinemu.dolphinemu.ui.main.ThemeProvider
import org.dolphinemu.dolphinemu.ui.theme.DolphinTheme
import org.dolphinemu.dolphinemu.ui.theme.MenuSpacer
import org.dolphinemu.dolphinemu.utils.ThemeHelper

private data class ErrorDialogState(val message: String) {
    val onDismissed = CompletableDeferred<Unit>()
}

class NetplaySetupActivity : AppCompatActivity(), ThemeProvider {
    override var themeId: Int = 0
    private lateinit var viewModel: NetplaySetupViewModel

    override fun onCreate(savedInstanceState: Bundle?) {
        ThemeHelper.setTheme(this)
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)

        Netplay.launchGame
            .onEach { EmulationActivity.launch(this, it, false) }
            .launchIn(lifecycleScope)

        viewModel = ViewModelProvider(this)[NetplaySetupViewModel::class.java]

        viewModel.showNetplayScreen
            .onEach { /* launch NetplayActivity */ }
            .launchIn(lifecycleScope)

        setContent {
            DolphinTheme {
                NetplaySetupScreen(
                    onBackClicked = { finish() },
                    connecting = viewModel.connecting.collectAsState().value,
                    errors = viewModel.errors,
                    nickname = viewModel.nickname.collectAsState().value,
                    onNicknameChanged = viewModel::setNickname,
                    connectionType = viewModel.connectionType.collectAsState().value,
                    onConnectionTypeChanged = viewModel::setConnectionType,
                    connectionRole = viewModel.connectionRole.collectAsState().value,
                    onConnectionRoleChanged = viewModel::setConnectionRole,
                    ipAddress = viewModel.ipAddress.collectAsState().value,
                    onIpAddressChanged = viewModel::setIpAddress,
                    connectPort = viewModel.connectPort.collectAsState().value,
                    onConnectPortChanged = viewModel::setConnectPort,
                    hostCode = viewModel.hostCode.collectAsState().value,
                    onHostCodeChanged = viewModel::setHostCode,
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

@Composable
private fun NetplaySetupScreen(
    onBackClicked: () -> Unit,
    connecting: Boolean,
    errors: Flow<String>,
    connectionRole: ConnectionRole,
    onConnectionRoleChanged: (ConnectionRole) -> Unit,
    nickname: String,
    onNicknameChanged: (String) -> Unit,
    connectionType: ConnectionType,
    onConnectionTypeChanged: (ConnectionType) -> Unit,
    ipAddress: String,
    onIpAddressChanged: (String) -> Unit,
    connectPort: String,
    onConnectPortChanged: (String) -> Unit,
    hostCode: String,
    onHostCodeChanged: (String) -> Unit,
    onConnectClicked: () -> Unit,
) {
    Scaffold(
        topBar = {
            MediumTopAppBar(
                title = { Text(stringResource(R.string.netplay_setup_title)) },
                navigationIcon = {
                    IconButton(onClick = onBackClicked) {
                        Icon(
                            Icons.AutoMirrored.Filled.ArrowBack,
                            contentDescription = "Back"
                        )
                    }
                },
            )
        },
        floatingActionButton = {
            ExtendedFloatingActionButton(
                onClick = onConnectClicked,
            ) {
                if (connecting) {
                    CircularProgressIndicator(
                        modifier = Modifier.size(18.dp),
                        strokeWidth = 2.dp,
                        color = MaterialTheme.colorScheme.onPrimaryContainer,
                    )
                    Spacer(Modifier.width(12.dp))
                    Text(stringResource(connectionRole.loadingLabelId))
                } else {
                    Text(stringResource(connectionRole.labelId))
                }
            }
        }
    ) { innerPadding ->
        var activeErrorDialog by remember { mutableStateOf<ErrorDialogState?>(null) }
        LaunchedEffect(Unit) {
            errors.collect { message ->
                activeErrorDialog = ErrorDialogState(message)
                activeErrorDialog?.onDismissed?.await()
                activeErrorDialog = null
            }
        }
        activeErrorDialog?.let { activeErrorDialog ->
            AlertDialog(
                text = { Text(activeErrorDialog.message) },
                confirmButton = {
                    TextButton(onClick = { activeErrorDialog.onDismissed.complete(Unit) }) {
                        Text("Dismiss")
                    }
                },
                onDismissRequest = { activeErrorDialog.onDismissed.complete(Unit) },
            )
        }

        Column(
            modifier = Modifier
                .fillMaxSize()
                .consumeWindowInsets(innerPadding)
                .verticalScroll(rememberScrollState())
                .padding(innerPadding)
        ) {
            SecondaryTabRow(selectedTabIndex = ConnectionRole.all.indexOf(connectionRole)) {
                ConnectionRole.all.forEach { role ->
                    Tab(
                        selected = connectionRole == role,
                        onClick = { onConnectionRoleChanged(role) },
                        text = { Text(stringResource(role.labelId)) },
                    )
                }
            }

            MenuSpacer()

            Column(
                modifier = Modifier
                    .padding(horizontal = DolphinTheme.scaffoldPadding)
            ) {
                NetplaySetupContent(
                    nickname = nickname,
                    onNicknameChanged = onNicknameChanged,
                    connectionType = connectionType,
                    onConnectionTypeChanged = onConnectionTypeChanged,
                    ipAddress = ipAddress,
                    onIpAddressChanged = onIpAddressChanged,
                    hostCode = hostCode,
                    onHostCodeChanged = onHostCodeChanged,
                    connectPort = connectPort,
                    onConnectPortChanged = onConnectPortChanged,
                    connectionRole = connectionRole,
                )
            }
        }
    }
}

@Composable
private fun NetplaySetupContent(
    nickname: String,
    onNicknameChanged: (String) -> Unit,
    connectionType: ConnectionType,
    onConnectionTypeChanged: (ConnectionType) -> Unit,
    ipAddress: String,
    onIpAddressChanged: (String) -> Unit,
    hostCode: String,
    onHostCodeChanged: (String) -> Unit,
    connectPort: String,
    onConnectPortChanged: (String) -> Unit,
    connectionRole: ConnectionRole,
) {
    OutlinedTextField(
        value = nickname,
        onValueChange = onNicknameChanged,
        label = { Text(stringResource(R.string.netplay_nickname_label)) },
        modifier = Modifier.fillMaxWidth()
    )

    MenuSpacer()

    ConnectionTypePicker(
        connectionType = connectionType,
        onConnectionTypeChanged = onConnectionTypeChanged,
    )

    MenuSpacer()

    when (connectionRole) {
        ConnectionRole.Connect -> ConnectMenu(
            connectionType = connectionType,
            ipAddress = ipAddress,
            onIpAddressChanged = onIpAddressChanged,
            hostCode = hostCode,
            onHostCodeChanged = onHostCodeChanged,
            port = connectPort,
            onPortChanged = onConnectPortChanged,
        )

        ConnectionRole.Host -> {}
    }
}

@Composable
private fun ConnectionTypePicker(
    connectionType: ConnectionType,
    onConnectionTypeChanged: (ConnectionType) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }

    ExposedDropdownMenuBox(
        expanded = expanded,
        onExpandedChange = { expanded = it },
    ) {
        ExposedDropdownMenu(
            expanded = expanded,
            onDismissRequest = { expanded = false },
        ) {
            ConnectionType.all.forEach { connectionType ->
                DropdownMenuItem(
                    text = { Text(stringResource(connectionType.labelId)) },
                    onClick = {
                        onConnectionTypeChanged(connectionType)
                        expanded = false
                    },
                    contentPadding = ExposedDropdownMenuDefaults.ItemContentPadding,
                )
            }
        }
        OutlinedTextField(
            value = stringResource(connectionType.labelId),
            onValueChange = {},
            readOnly = true,
            label = { Text(stringResource(R.string.netplay_connection_type)) },
            trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded) },
            modifier = Modifier
                .menuAnchor(MenuAnchorType.PrimaryNotEditable)
                .fillMaxWidth()
        )
    }
}

@Composable
fun ConnectMenu(
    connectionType: ConnectionType,
    ipAddress: String,
    onIpAddressChanged: (String) -> Unit,
    hostCode: String,
    onHostCodeChanged: (String) -> Unit,
    port: String,
    onPortChanged: (String) -> Unit,
) {
    when (connectionType) {
        ConnectionType.DirectConnection -> {
            OutlinedTextField(
                value = ipAddress,
                onValueChange = onIpAddressChanged,
                label = { Text(stringResource(R.string.netplay_ip_address_label)) },
                keyboardOptions = KeyboardOptions(
                    keyboardType = KeyboardType.Number,
                ),
                modifier = Modifier
                    .fillMaxWidth()
            )

            MenuSpacer()

            OutlinedTextField(
                value = port,
                onValueChange = onPortChanged,
                label = { Text(stringResource(R.string.netplay_port_label)) },
                keyboardOptions = KeyboardOptions(
                    keyboardType = KeyboardType.Number,
                ),
                modifier = Modifier
                    .fillMaxWidth()
            )
        }

        ConnectionType.TraversalServer -> OutlinedTextField(
            value = hostCode,
            onValueChange = onHostCodeChanged,
            label = { Text(stringResource(R.string.netplay_host_code_label)) },
            modifier = Modifier
                .fillMaxWidth()
        )
    }
}

@Preview
@Composable
private fun NetplaySetupScreenPreview() {
    MaterialTheme {
        NetplaySetupScreen(
            onBackClicked = {},
            connecting = false,
            errors = emptyFlow(),
            connectionRole = ConnectionRole.Connect,
            onConnectionRoleChanged = {},
            nickname = "Preview nickname",
            onNicknameChanged = {},
            connectionType = ConnectionType.DirectConnection,
            onConnectionTypeChanged = {},
            ipAddress = "127.0.0.1",
            onIpAddressChanged = {},
            connectPort = "2626",
            onConnectPortChanged = {},
            hostCode = "",
            onHostCodeChanged = {},
            onConnectClicked = {},
        )
    }
}
