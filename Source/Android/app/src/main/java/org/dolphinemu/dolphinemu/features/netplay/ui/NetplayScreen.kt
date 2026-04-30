// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.netplay.ui

import android.content.res.Configuration
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.consumeWindowInsets
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListScope
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Remove
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.LocalTextStyle
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.MediumTopAppBar
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.TextRange
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.TextFieldValue
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.emptyFlow
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.features.netplay.model.GameDigestProgress
import org.dolphinemu.dolphinemu.features.netplay.model.NetplayMessage
import org.dolphinemu.dolphinemu.features.netplay.model.Player
import org.dolphinemu.dolphinemu.features.netplay.model.SaveTransferProgress
import org.dolphinemu.dolphinemu.ui.theme.DolphinTheme
import org.dolphinemu.dolphinemu.ui.theme.MenuSpacer
import org.dolphinemu.dolphinemu.ui.theme.OutlinedBox
import org.dolphinemu.dolphinemu.ui.theme.PreviewTheme
import java.util.Locale

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun NetplayScreen(
    onBackClicked: () -> Unit,
    connectionLost: Flow<Unit>,
    messages: List<NetplayMessage>,
    onSendMessage: (String) -> Unit,
    game: String,
    hostInputAuthorityEnabled: Boolean,
    maxBuffer: Int,
    onMaxBufferChanged: (Int) -> Unit,
    players: List<Player>,
    saveTransferProgress: SaveTransferProgress?,
    gameDigestProgress: GameDigestProgress?,
) {
    Scaffold(
        topBar = {
            MediumTopAppBar(
                title = { Text(stringResource(R.string.netplay_title)) },
                navigationIcon = {
                    IconButton(onClick = onBackClicked) {
                        Icon(
                            imageVector = Icons.AutoMirrored.Filled.ArrowBack,
                            contentDescription = "Back",
                        )
                    }
                },
            )
        },
    ) { innerPadding ->
        val modifier = Modifier
            .fillMaxSize()
            .consumeWindowInsets(innerPadding)
            .padding(innerPadding)

        if (LocalConfiguration.current.orientation == Configuration.ORIENTATION_LANDSCAPE) {
            LandscapeContent(
                messages = messages,
                onSendMessage = onSendMessage,
                game = game,
                players = players,
                hostInputAuthorityEnabled = hostInputAuthorityEnabled,
                maxBuffer = maxBuffer,
                onMaxBufferChanged = onMaxBufferChanged,
                modifier = modifier
            )
        } else {
            PortraitContent(
                messages = messages,
                onSendMessage = onSendMessage,
                game = game,
                players = players,
                hostInputAuthorityEnabled = hostInputAuthorityEnabled,
                maxBuffer = maxBuffer,
                onMaxBufferChanged = onMaxBufferChanged,
                modifier = modifier
            )
        }

        var showConnectionLostDialog by rememberSaveable { mutableStateOf(false) }
        LaunchedEffect(Unit) {
            connectionLost.collect { showConnectionLostDialog = true }
        }

        var dismissSaveTransferProgressDialog by rememberSaveable { mutableStateOf(false) }
        if (saveTransferProgress == null) {
            dismissSaveTransferProgressDialog = false
        }

        var dismissGameDigestDialog by rememberSaveable { mutableStateOf(false) }
        if (gameDigestProgress == null) {
            dismissGameDigestDialog = false
        }

        when {
            showConnectionLostDialog -> {
                AlertDialog(
                    text = { Text(stringResource(R.string.netplay_connection_lost)) },
                    confirmButton = {
                        TextButton(onClick = onBackClicked) {
                            Text(stringResource(R.string.ok))
                        }
                    },
                    onDismissRequest = onBackClicked,
                )
            }

            saveTransferProgress != null && !dismissSaveTransferProgressDialog -> {
                SaveTransferProgressDialog(
                    saveTransferProgress = saveTransferProgress,
                    onDismiss = { dismissSaveTransferProgressDialog = true },
                )
            }

            gameDigestProgress != null && !dismissGameDigestDialog -> {
                GameDigestProgressDialog(
                    gameDigestProgress = gameDigestProgress,
                    onDismiss = { dismissGameDigestDialog = true },
                )
            }
        }
    }
}

@Composable
private fun PortraitContent(
    messages: List<NetplayMessage>,
    onSendMessage: (String) -> Unit,
    game: String,
    players: List<Player>,
    hostInputAuthorityEnabled: Boolean,
    maxBuffer: Int,
    onMaxBufferChanged: (Int) -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier
    ) {
        Chat(
            messages = messages,
            onSendMessage = onSendMessage,
            modifier = Modifier
                .fillMaxWidth()
                .fillMaxHeight(0.3f)
                .padding(horizontal = DolphinTheme.scaffoldPadding)
        )

        MenuSpacer()

        PLayersAndSettings(
            game = game,
            players = players,
            hostInputAuthorityEnabled = hostInputAuthorityEnabled,
            maxBuffer = maxBuffer,
            onMaxBufferChanged = onMaxBufferChanged,
            modifier = Modifier
                .weight(1f)
                .padding(horizontal = DolphinTheme.scaffoldPadding),
        )
    }
}

@Composable
private fun LandscapeContent(
    messages: List<NetplayMessage>,
    onSendMessage: (String) -> Unit,
    game: String,
    players: List<Player>,
    hostInputAuthorityEnabled: Boolean,
    maxBuffer: Int,
    onMaxBufferChanged: (Int) -> Unit,
    modifier: Modifier = Modifier,
) {
    Row(
        modifier = modifier
    ) {
        Chat(
            messages = messages,
            onSendMessage = onSendMessage,
            modifier = Modifier
                .weight(1f)
                .fillMaxHeight()
                .padding(horizontal = DolphinTheme.scaffoldPadding)
        )

        PLayersAndSettings(
            game = game,
            players = players,
            hostInputAuthorityEnabled = hostInputAuthorityEnabled,
            maxBuffer = maxBuffer,
            onMaxBufferChanged = onMaxBufferChanged,
            modifier = Modifier
                .weight(1f)
                .padding(horizontal = DolphinTheme.scaffoldPadding)
        )
    }
}

@Composable
private fun PLayersAndSettings(
    game: String,
    players: List<Player>,
    hostInputAuthorityEnabled: Boolean,
    maxBuffer: Int,
    onMaxBufferChanged: (Int) -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier
            .verticalScroll(rememberScrollState())
    ) {
        OutlinedTextField(
            value = game,
            onValueChange = {},
            label = { Text(stringResource(R.string.netplay_game_label)) },
            readOnly = true,
            modifier = Modifier
                .fillMaxWidth()
        )

        MenuSpacer()

        OutlinedBox(
            label = { Text(stringResource(R.string.netplay_players_label)) },
        ) {
            PlayersTable(
                rows = buildList {
                    add(
                        listOf(
                            stringResource(R.string.netplay_players_name),
                            stringResource(R.string.netplay_players_ping),
                            stringResource(R.string.netplay_players_mapping),
                        )
                    )
                    addAll(players.map { listOf(it.name, it.ping.toString(), it.mapping) })
                    repeat(4 - players.size) { add(listOf("", "", "")) }
                },
                modifier = Modifier
                    .fillMaxWidth()
            )
        }

        if (hostInputAuthorityEnabled) {
            MenuSpacer()

            BufferInput(
                value = maxBuffer,
                onValueChange = onMaxBufferChanged,
                label = stringResource(R.string.netplay_max_buffer),
            )
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun Chat(
    messages: List<NetplayMessage>,
    onSendMessage: (String) -> Unit,
    modifier: Modifier,
) {
    val context = LocalContext.current

    fun LazyListScope.messages() {
        items(messages.size) { index ->
            Text(text = messages[index].message(context))
        }
    }

    var draftMessage by remember { mutableStateOf("") }
    val submitMessage = {
        onSendMessage(draftMessage)
        draftMessage = ""
    }

    var showBottomSheet by remember { mutableStateOf(false) }
    val bottomSheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)

    if (showBottomSheet) {
        ModalBottomSheet(
            onDismissRequest = { showBottomSheet = false },
            sheetState = bottomSheetState,
            modifier = Modifier
                .statusBarsPadding()
        ) {
            LazyColumn(
                reverseLayout = true,
                contentPadding = PaddingValues(bottom = 4.dp),
                modifier = Modifier
                    .weight(1f)
                    .padding(horizontal = DolphinTheme.scaffoldPadding)
            ) {
                messages()
            }

            Row(
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 8.dp)
            ) {
                OutlinedTextField(
                    value = draftMessage,
                    onValueChange = { draftMessage = it },
                    keyboardOptions = KeyboardOptions(imeAction = ImeAction.Send),
                    keyboardActions = KeyboardActions(onSend = { submitMessage() }),
                    modifier = Modifier
                        .weight(1f)
                )
                TextButton(
                    onClick = submitMessage,
                    enabled = draftMessage.isNotBlank(),
                ) {
                    Text(stringResource(R.string.netplay_chat_send))
                }
            }
        }
    }

    OutlinedBox(
        onClick = { showBottomSheet = true },
        label = { Text(stringResource(R.string.netplay_chat_label)) },
        modifier = modifier
    ) {
        LazyColumn(
            reverseLayout = true,
            userScrollEnabled = false,
            modifier = Modifier
                .fillMaxSize()
        ) {
            messages()
        }
    }
}

/**
 * A table arranged into columns sized to wrap the largest item. Except the
 * first column which takes up the remaining space left by the other columns.
 * The first row is treated as the column titles.
 */
@Composable
private fun PlayersTable(
    rows: List<List<String>>,
    modifier: Modifier = Modifier,
) {
    rows.zipWithNext { a, b -> if (a.size != b.size) throw IllegalArgumentException("Rows must all contain the same number of elements.") }
    val maxWidths = remember { List(rows.first().size) { mutableIntStateOf(0) } }
    val density = LocalDensity.current

    Column(
        verticalArrangement = Arrangement.spacedBy(6.dp),
        modifier = modifier
    ) {
        rows.forEachIndexed { rowIndex, row ->
            Row(
                horizontalArrangement = Arrangement.spacedBy(16.dp),
            ) {
                row.forEachIndexed { itemIndex, text ->
                    Box(
                        modifier = Modifier
                            .then(
                                when {
                                    itemIndex == 0 -> Modifier.weight(1f)

                                    maxWidths[itemIndex].intValue > 0 -> Modifier
                                        .width(with(density) { maxWidths[itemIndex].intValue.toDp() })

                                    else -> Modifier
                                }
                            )
                            .onGloballyPositioned { coordinates ->
                                val width = coordinates.size.width
                                if (width > maxWidths[itemIndex].intValue) {
                                    maxWidths[itemIndex].intValue = width
                                }
                            }
                    ) {
                        Text(
                            text = text,
                            fontWeight = if (rowIndex == 0) FontWeight.Medium else FontWeight.Normal,
                            style = MaterialTheme.typography.bodyMedium,
                        )
                    }
                }
            }
            if (rowIndex == 0) {
                HorizontalDivider()
            }
        }
    }
}

@Composable
private fun BufferInput(
    value: Int,
    onValueChange: (Int) -> Unit,
    label: String,
) {
    val range = 0..99
    var maybeEmptyValue by remember(value) {
        mutableStateOf("$value")
    }

    Row(
        verticalAlignment = Alignment.CenterVertically,
        modifier = Modifier
            .fillMaxWidth()
    ) {
        OutlinedTextField(
            value = TextFieldValue(
                text = maybeEmptyValue,
                selection = TextRange(maybeEmptyValue.length)
            ),
            onValueChange = { newValue ->
                if (newValue.text.isEmpty()) {
                    maybeEmptyValue = newValue.text
                    return@OutlinedTextField
                }
                newValue.text.toIntOrNull()?.let {
                    if (it in range) {
                        onValueChange(it)
                    }
                }
            },
            label = { Text(label) },
            textStyle = LocalTextStyle.current.copy(
                textAlign = TextAlign.Center
            ),
            keyboardOptions = KeyboardOptions(
                keyboardType = KeyboardType.Number,
            ),
            singleLine = true,
            modifier = Modifier
                .weight(1f)
        )

        Spacer(modifier = Modifier.width(12.dp))

        Button(
            onClick = {
                if (maybeEmptyValue.isEmpty()) {
                    maybeEmptyValue = "0"
                    onValueChange(0)
                } else {
                    val newValue = value - 1
                    if (newValue in range) {
                        onValueChange(newValue)
                    }
                }
            },
            shape = RoundedCornerShape(
                topStartPercent = 50,
                topEndPercent = 0,
                bottomEndPercent = 0,
                bottomStartPercent = 50,
            ),
            modifier = Modifier
                .height(60.dp)
                .padding(top = 8.dp)
        ) {
            Icon(Icons.Filled.Remove, contentDescription = "Back")
        }

        Spacer(modifier = Modifier.width(2.dp))

        Button(
            onClick = {
                if (maybeEmptyValue.isEmpty()) {
                    maybeEmptyValue = "0"
                    onValueChange(0)
                } else {
                    val newValue = value + 1
                    if (newValue in range) {
                        onValueChange(newValue)
                    }
                }
            },
            shape = RoundedCornerShape(
                topStartPercent = 0,
                topEndPercent = 50,
                bottomEndPercent = 50,
                bottomStartPercent = 0,
            ),
            modifier = Modifier
                .height(60.dp)
                .padding(top = 8.dp)
        ) {
            Icon(Icons.Filled.Add, contentDescription = "Back")
        }
    }
}

@Composable
private fun SaveTransferProgressDialog(
    saveTransferProgress: SaveTransferProgress,
    onDismiss: () -> Unit,
) {
    AlertDialog(
        title = { Text(saveTransferProgress.title) },
        text = {
            Column(
                verticalArrangement = Arrangement.spacedBy(12.dp),
                modifier = Modifier.verticalScroll(rememberScrollState()),
            ) {
                saveTransferProgress.playerProgresses.forEachIndexed { index, playerProgress ->
                    SaveTransferProgressRow(
                        playerProgress = playerProgress,
                        totalSize = saveTransferProgress.totalSize,
                    )

                    if (index < saveTransferProgress.playerProgresses.lastIndex) {
                        HorizontalDivider()
                    }
                }
            }
        },
        confirmButton = {
            TextButton(onClick = onDismiss) {
                Text(stringResource(R.string.netplay_save_transfer_progress_close))
            }
        },
        onDismissRequest = onDismiss,
    )
}

@Composable
private fun SaveTransferProgressRow(
    playerProgress: SaveTransferProgress.PlayerProgress,
    totalSize: Long,
) {
    fun formatMib(bytes: Long) = String.format(Locale.US, "%.2f", bytes / 1024f / 1024f)
    val progressFraction = (playerProgress.progress.toFloat() / totalSize).coerceIn(0f, 1f)

    Column(
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        LinearProgressIndicator(
            progress = { progressFraction },
            modifier = Modifier.fillMaxWidth(),
        )
        Row(
            modifier = Modifier.fillMaxWidth(),
        ) {
            Text(
                text = playerProgress.name,
                modifier = Modifier.weight(1f),
            )
            Spacer(modifier = Modifier.width(16.dp))
            Text(
                text = "${formatMib(playerProgress.progress)}/${formatMib(totalSize)} MiB",
            )
        }
    }
}

@Composable
private fun GameDigestProgressDialog(
    gameDigestProgress: GameDigestProgress,
    onDismiss: () -> Unit,
) {
    AlertDialog(
        title = { Text(gameDigestProgress.title) },
        text = {
            Column(
                verticalArrangement = Arrangement.spacedBy(12.dp),
                modifier = Modifier.verticalScroll(rememberScrollState()),
            ) {
                gameDigestProgress.playerProgresses.forEachIndexed { index, playerProgress ->
                    GameDigestPlayerRow(playerProgress)
                    if (index < gameDigestProgress.playerProgresses.lastIndex) {
                        HorizontalDivider()
                    }
                }
                if (gameDigestProgress.matches != null) {
                    Spacer(modifier = Modifier.height(4.dp))
                    Text(
                        text = stringResource(
                            if (gameDigestProgress.matches) {
                                R.string.netplay_game_digest_match
                            } else {
                                R.string.netplay_game_digest_mismatch
                            }
                        ),
                        style = MaterialTheme.typography.bodyLarge,
                        fontWeight = FontWeight.Medium,
                    )
                }
            }
        },
        confirmButton = {
            if (gameDigestProgress.matches != null) {
                TextButton(onClick = onDismiss) {
                    Text(stringResource(R.string.netplay_game_digest_close))
                }
            }
        },
        onDismissRequest = { onDismiss() },
    )
}

@Composable
private fun GameDigestPlayerRow(
    playerProgress: GameDigestProgress.PlayerProgress,
) {
    Column(
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        LinearProgressIndicator(
            progress = { playerProgress.progress / 100f },
            modifier = Modifier.fillMaxWidth(),
        )
        Row(
            modifier = Modifier.fillMaxWidth(),
        ) {
            if (playerProgress.result == null) {
                Text(
                    text = playerProgress.name,
                )
                Spacer(modifier = Modifier.weight(1f))
                Text(
                    text = "${playerProgress.progress}%",
                )
            } else {
                Text(
                    text = "${playerProgress.name}:\u00A0${playerProgress.result}",
                )
            }
        }
    }
}

@Preview
@Composable
private fun NetplayScreenPreview() {
    PreviewTheme(darkTheme = false) {
        PreviewNetplayScreen()
    }
}

@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES)
@Composable
private fun NetplayScreenDarkPreview() {
    PreviewTheme(darkTheme = true) {
        PreviewNetplayScreen()
    }
}

@Preview(widthDp = 891, heightDp = 411)
@Composable
private fun LandscapeNetplayScreenPreview() {
    PreviewTheme(darkTheme = false) {
        PreviewNetplayScreen()
    }
}

@Preview(
    widthDp = 891,
    heightDp = 411,
    uiMode = Configuration.UI_MODE_NIGHT_YES
)

@Composable
private fun LandscapeNetplayScreenDarkPreview() {
    PreviewTheme(darkTheme = true) {
        PreviewNetplayScreen()
    }
}

@Composable
private fun PreviewNetplayScreen() {
    NetplayScreen(
        onBackClicked = {},
        connectionLost = emptyFlow(),
        players = listOf(
            Player(
                pid = 1,
                name = "Player 1",
                revision = "123",
                ping = 2,
                isHost = true,
                mapping = "m1"
            ),
            Player(
                pid = 2,
                name = "Player 2",
                revision = "123",
                ping = 23,
                isHost = false,
                mapping = "m2"
            ),
        ),
        messages = buildList {
            repeat(5) {
                add(NetplayMessage.Chat("Hello"))
            }
        },
        onSendMessage = {},
        game = "Game name",
        hostInputAuthorityEnabled = true,
        maxBuffer = 10,
        onMaxBufferChanged = {},
        saveTransferProgress = null,
        gameDigestProgress = null,
//        saveTransferProgress = SaveTransferProgress(
//            title = "Title",
//            totalSize = 1024L,
//            playerProgresses = listOf(
//                SaveTransferProgress.PlayerProgress(
//                    playerId = 1,
//                    name = "Player 1",
//                    progress = 256,
//                ),
//                SaveTransferProgress.PlayerProgress(
//                    playerId = 2,
//                    name = "Player 2",
//                    progress = 512,
//                ),
//            ),
//        ),
    )
}
