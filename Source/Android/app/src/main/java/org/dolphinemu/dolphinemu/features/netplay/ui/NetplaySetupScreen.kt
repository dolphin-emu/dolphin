@file:OptIn(ExperimentalMaterial3Api::class)

package org.dolphinemu.dolphinemu.features.netplay.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.consumeWindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Checkbox
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.ExposedDropdownMenuDefaults
import androidx.compose.material3.ExtendedFloatingActionButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.MediumTopAppBar
import androidx.compose.material3.MenuAnchorType
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SecondaryTabRow
import androidx.compose.material3.Tab
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.emptyFlow
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.features.netplay.model.ConnectionRole
import org.dolphinemu.dolphinemu.features.netplay.model.ConnectionType
import org.dolphinemu.dolphinemu.ui.theme.DolphinTheme
import org.dolphinemu.dolphinemu.ui.theme.MenuSpacer

private data class ErrorDialogState(val message: String) {
    val onDismissed = CompletableDeferred<Unit>()
}

@Composable
fun NetplaySetupScreen(
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
    hostPort: String,
    onHostPortChanged: (String) -> Unit,
    useUpnp: Boolean,
    onUseUpnpChanged: (Boolean) -> Unit,
    onHostClicked: () -> Unit,
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
                onClick = when(connectionRole) {
                    ConnectionRole.Host -> onHostClicked
                    ConnectionRole.Connect -> onConnectClicked
                },
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
                .padding(bottom = DolphinTheme.fabClearancePadding)
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
                    hostPort = hostPort,
                    onHostPortChanged = onHostPortChanged,
                    useUpnp = useUpnp,
                    onUseUpnpChanged = onUseUpnpChanged,
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
    hostPort: String,
    onHostPortChanged: (String) -> Unit,
    useUpnp: Boolean,
    onUseUpnpChanged: (Boolean) -> Unit,
    connectionRole: ConnectionRole,
) {
    OutlinedTextField(
        value = nickname,
        onValueChange = onNicknameChanged,
        label = { Text(stringResource(R.string.netplay_nickname_label)) },
        singleLine = true,
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

        ConnectionRole.Host -> HostMenu(
            connectionType = connectionType,
            hostPort = hostPort,
            onHostPortChanged = onHostPortChanged,
            useUpnp = useUpnp,
            onUseUpnpChanged = onUseUpnpChanged,
        )
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
                singleLine = true,
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
                singleLine = true,
                modifier = Modifier
                    .fillMaxWidth()
            )
        }

        ConnectionType.TraversalServer -> OutlinedTextField(
            value = hostCode,
            onValueChange = onHostCodeChanged,
            label = { Text(stringResource(R.string.netplay_host_code_label)) },
            singleLine = true,
            modifier = Modifier
                .fillMaxWidth()
        )
    }
}

@Composable
private fun HostMenu(
    connectionType: ConnectionType,
    hostPort: String,
    onHostPortChanged: (String) -> Unit,
    useUpnp: Boolean,
    onUseUpnpChanged: (Boolean) -> Unit,
) {
    if (connectionType == ConnectionType.DirectConnection) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.fillMaxWidth(),
        ) {
            OutlinedTextField(
                value = hostPort,
                onValueChange = onHostPortChanged,
                label = { Text(stringResource(R.string.netplay_host_port_label)) },
                keyboardOptions = KeyboardOptions(
                    keyboardType = KeyboardType.Number,
                ),
                singleLine = true,
                modifier = Modifier
                    .weight(1f)
            )

            Spacer(modifier = Modifier.width(8.dp))

            OutlinedButton(
                onClick = { onUseUpnpChanged(!useUpnp) },
                shape = MaterialTheme.shapes.extraSmall,
                modifier = Modifier
                    .height(64.dp)
                    .padding(top = 8.dp)
            ) {
                Text(
                    text = stringResource(R.string.netplay_use_upnp),
                )
                Spacer(modifier = Modifier.width(12.dp))
                Checkbox(
                    checked = useUpnp,
                    onCheckedChange = null,
                    modifier = Modifier.size(24.dp),
                )
            }
        }

        MenuSpacer()
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
            connectionRole = ConnectionRole.Host,
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
            hostPort = "2626",
            onHostPortChanged = {},
            useUpnp = false,
            onUseUpnpChanged = {},
            onHostClicked = {},
            onConnectClicked = {},
        )
    }
}
