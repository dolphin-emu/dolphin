// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.netplay.ui

import android.content.Intent
import android.content.res.Configuration
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.consumeWindowInsets
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListScope
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Remove
import androidx.compose.material.icons.filled.Share
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.ExposedDropdownMenuDefaults
import androidx.compose.material3.ExtendedFloatingActionButton
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.LocalTextStyle
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.MediumTopAppBar
import androidx.compose.material3.MenuAnchorType
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SheetState
import androidx.compose.material3.SheetValue
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
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
import androidx.compose.ui.layout.ContentScale
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
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import coil.compose.AsyncImage
import coil.request.ImageRequest
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.emptyFlow
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.features.netplay.model.GameDigestProgress
import org.dolphinemu.dolphinemu.features.netplay.model.JoinAddress
import org.dolphinemu.dolphinemu.features.netplay.model.JoinInfoType
import org.dolphinemu.dolphinemu.features.netplay.model.NetplayMessage
import org.dolphinemu.dolphinemu.features.netplay.model.Player
import org.dolphinemu.dolphinemu.features.netplay.model.SaveTransferProgress
import org.dolphinemu.dolphinemu.features.netplay.model.TraversalState
import org.dolphinemu.dolphinemu.model.GameFile
import org.dolphinemu.dolphinemu.ui.theme.DolphinTheme
import org.dolphinemu.dolphinemu.ui.theme.MenuSpacer
import org.dolphinemu.dolphinemu.ui.theme.OutlinedBox
import org.dolphinemu.dolphinemu.ui.theme.PreviewTheme
import org.dolphinemu.dolphinemu.ui.theme.ReadOnlyTextField
import org.dolphinemu.dolphinemu.utils.CoilUtils
import java.util.Locale

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun NetplayScreen(
    onBackClicked: () -> Unit,
    isHosting: Boolean,
    connectionLost: Flow<Unit>,
    fatalTraversalError: Flow<TraversalState.Failure>,
    messages: List<NetplayMessage>,
    onSendMessage: (String) -> Unit,
    game: String,
    onStartGame: () -> Unit,
    onGameSelected: (GameFile) -> Unit,
    gameFiles: List<GameFile>,
    hostInputAuthorityEnabled: Boolean,
    maxBuffer: Int,
    onMaxBufferChanged: (Int) -> Unit,
    players: List<Player>,
    saveTransferProgress: SaveTransferProgress?,
    gameDigestProgress: GameDigestProgress?,
    joinAddresses: Map<JoinInfoType, JoinAddress>,
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
        floatingActionButton = {
            if (isHosting) {
                ExtendedFloatingActionButton(onClick = onStartGame) {
                    Text(stringResource(R.string.netplay_start))
                }
            }
        },
    ) { innerPadding ->
        val modifier = Modifier
            .fillMaxSize()
            .consumeWindowInsets(innerPadding)
            .padding(innerPadding)

        // State which must live above the landscape/portrait split.
        var showChat by rememberSaveable { mutableStateOf(false) }
        var showGamePicker by rememberSaveable { mutableStateOf(false) }
        var selectedJoinInfoType by rememberSaveable {
            mutableStateOf(joinAddresses.keys.firstOrNull() ?: JoinInfoType.EXTERNAL)
        }

        if (LocalConfiguration.current.orientation == Configuration.ORIENTATION_LANDSCAPE) {
            LandscapeContent(
                isHosting = isHosting,
                messages = messages,
                onSendMessage = onSendMessage,
                showChat = showChat,
                onShowChatChanged = { showChat = it },
                game = game,
                gameFiles = gameFiles,
                onGameSelected = onGameSelected,
                showGamePicker = showGamePicker,
                onShowGamePickerChanged = { showGamePicker = it },
                players = players,
                hostInputAuthorityEnabled = hostInputAuthorityEnabled,
                maxBuffer = maxBuffer,
                onMaxBufferChanged = onMaxBufferChanged,
                joinAddresses = joinAddresses,
                selectedJoinInfoType = selectedJoinInfoType,
                onSelectedJoinInfoTypeChanged = { selectedJoinInfoType = it },
                modifier = modifier
            )
        } else {
            PortraitContent(
                isHosting = isHosting,
                messages = messages,
                onSendMessage = onSendMessage,
                showChat = showChat,
                onShowChatChanged = { showChat = it },
                game = game,
                gameFiles = gameFiles,
                onGameSelected = onGameSelected,
                showGamePicker = showGamePicker,
                onShowGamePickerChanged = { showGamePicker = it },
                players = players,
                hostInputAuthorityEnabled = hostInputAuthorityEnabled,
                maxBuffer = maxBuffer,
                onMaxBufferChanged = onMaxBufferChanged,
                joinAddresses = joinAddresses,
                selectedJoinInfoType = selectedJoinInfoType,
                onSelectedJoinInfoTypeChanged = { selectedJoinInfoType = it },
                modifier = modifier
            )
        }

        var showConnectionLostDialog by rememberSaveable { mutableStateOf(false) }
        LaunchedEffect(Unit) {
            connectionLost.collect { showConnectionLostDialog = true }
        }

        var traversalError by rememberSaveable { mutableStateOf<TraversalState.Failure?>(null) }
        LaunchedEffect(Unit) {
            fatalTraversalError.collect { traversalError = it }
        }

        var dismissSaveTransferProgressDialog by rememberSaveable { mutableStateOf(false) }
        if (saveTransferProgress == null) {
            dismissSaveTransferProgressDialog = false
        }

        var dismissGameDigestDialog by rememberSaveable { mutableStateOf(false) }
        if (gameDigestProgress == null) {
            dismissGameDigestDialog = false
        }

        val currentTraversalError = traversalError

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

            currentTraversalError != null -> {
                AlertDialog(
                    text = { Text(currentTraversalError.message(LocalContext.current)) },
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
    isHosting: Boolean,
    messages: List<NetplayMessage>,
    onSendMessage: (String) -> Unit,
    showChat: Boolean,
    onShowChatChanged: (Boolean) -> Unit,
    game: String,
    gameFiles: List<GameFile>,
    onGameSelected: (GameFile) -> Unit,
    showGamePicker: Boolean,
    onShowGamePickerChanged: (Boolean) -> Unit,
    players: List<Player>,
    hostInputAuthorityEnabled: Boolean,
    maxBuffer: Int,
    onMaxBufferChanged: (Int) -> Unit,
    joinAddresses: Map<JoinInfoType, JoinAddress>,
    selectedJoinInfoType: JoinInfoType,
    onSelectedJoinInfoTypeChanged: (JoinInfoType) -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier
            .verticalScroll(rememberScrollState())
    ) {
        Chat(
            messages = messages,
            onSendMessage = onSendMessage,
            showBottomSheet = showChat,
            onShowBottomSheetChanged = onShowChatChanged,
            modifier = Modifier
                .fillMaxWidth()
                .height(200.dp)
                .padding(horizontal = DolphinTheme.scaffoldPadding)
        )

        MenuSpacer()

        PLayersAndSettings(
            game = game,
            gameFiles = gameFiles,
            onGameSelected = onGameSelected,
            showGamePicker = showGamePicker,
            onShowGamePickerChanged = onShowGamePickerChanged,
            players = players,
            hostInputAuthorityEnabled = hostInputAuthorityEnabled,
            maxBuffer = maxBuffer,
            onMaxBufferChanged = onMaxBufferChanged,
            isHosting = isHosting,
            joinAddresses = joinAddresses,
            selectedJoinInfoType = selectedJoinInfoType,
            onSelectedJoinInfoTypeChanged = onSelectedJoinInfoTypeChanged,
            modifier = Modifier
                .padding(horizontal = DolphinTheme.scaffoldPadding),
        )

        if (isHosting) {
            Spacer(modifier = Modifier.height(DolphinTheme.fabClearancePadding))
        }
    }
}

@Composable
private fun LandscapeContent(
    isHosting: Boolean,
    messages: List<NetplayMessage>,
    onSendMessage: (String) -> Unit,
    showChat: Boolean,
    onShowChatChanged: (Boolean) -> Unit,
    game: String,
    gameFiles: List<GameFile>,
    onGameSelected: (GameFile) -> Unit,
    showGamePicker: Boolean,
    onShowGamePickerChanged: (Boolean) -> Unit,
    players: List<Player>,
    hostInputAuthorityEnabled: Boolean,
    maxBuffer: Int,
    onMaxBufferChanged: (Int) -> Unit,
    joinAddresses: Map<JoinInfoType, JoinAddress>,
    selectedJoinInfoType: JoinInfoType,
    onSelectedJoinInfoTypeChanged: (JoinInfoType) -> Unit,
    modifier: Modifier = Modifier,
) {
    Row(
        modifier = modifier
            .padding(horizontal = DolphinTheme.scaffoldPadding)
    ) {
        Chat(
            messages = messages,
            onSendMessage = onSendMessage,
            showBottomSheet = showChat,
            onShowBottomSheetChanged = onShowChatChanged,
            modifier = Modifier
                .weight(1f)
                .fillMaxHeight()
        )

        Spacer(modifier = Modifier.width(16.dp))

        Column(
            modifier = Modifier
                .weight(1f)
                .verticalScroll(rememberScrollState())
        ) {
            PLayersAndSettings(
                game = game,
                gameFiles = gameFiles,
                onGameSelected = onGameSelected,
                showGamePicker = showGamePicker,
                onShowGamePickerChanged = onShowGamePickerChanged,
                players = players,
                hostInputAuthorityEnabled = hostInputAuthorityEnabled,
                maxBuffer = maxBuffer,
                onMaxBufferChanged = onMaxBufferChanged,
                isHosting = isHosting,
                joinAddresses = joinAddresses,
                selectedJoinInfoType = selectedJoinInfoType,
                onSelectedJoinInfoTypeChanged = onSelectedJoinInfoTypeChanged,
                modifier = Modifier
            )

            if (isHosting) {
                Spacer(modifier = Modifier.height(DolphinTheme.fabClearancePadding))
            }
        }
    }
}

@Composable
private fun PLayersAndSettings(
    game: String,
    gameFiles: List<GameFile>,
    onGameSelected: (GameFile) -> Unit,
    showGamePicker: Boolean,
    onShowGamePickerChanged: (Boolean) -> Unit,
    players: List<Player>,
    hostInputAuthorityEnabled: Boolean,
    maxBuffer: Int,
    onMaxBufferChanged: (Int) -> Unit,
    isHosting: Boolean,
    joinAddresses: Map<JoinInfoType, JoinAddress>,
    selectedJoinInfoType: JoinInfoType,
    onSelectedJoinInfoTypeChanged: (JoinInfoType) -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier
    ) {
        GamePicker(
            game = game,
            gameFiles = gameFiles,
            onGameSelected = onGameSelected,
            showGamePicker = showGamePicker,
            onShowGamePickerChanged = onShowGamePickerChanged,
            isHosting = isHosting,
        )

        if (isHosting) {
            MenuSpacer()

            JoinAddressSection(
                joinAddresses = joinAddresses,
                selectedType = selectedJoinInfoType,
                onSelectedTypeChanged = onSelectedJoinInfoTypeChanged,
            )
        }

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
    showBottomSheet: Boolean,
    onShowBottomSheetChanged: (Boolean) -> Unit,
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

    val density = LocalDensity.current
    val bottomSheetState = remember {
        SheetState(
            skipPartiallyExpanded = true,
            density = density,
            initialValue = if (showBottomSheet) SheetValue.Expanded else SheetValue.Hidden,
        )
    }

    if (showBottomSheet) {
        ModalBottomSheet(
            onDismissRequest = { onShowBottomSheetChanged(false) },
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
        onClick = { onShowBottomSheetChanged(true) },
        label = { Text(stringResource(R.string.netplay_chat_label)) },
        fadeContentTop = true,
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

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun GamePicker(
    game: String,
    gameFiles: List<GameFile>,
    onGameSelected: (GameFile) -> Unit,
    showGamePicker: Boolean,
    onShowGamePickerChanged: (Boolean) -> Unit,
    isHosting: Boolean,
) {
    val density = LocalDensity.current
    val bottomSheetState = remember {
        SheetState(
            skipPartiallyExpanded = true,
            density = density,
            initialValue = if (showGamePicker) SheetValue.Expanded else SheetValue.Hidden,
        )
    }

    if (showGamePicker) {
        ModalBottomSheet(
            onDismissRequest = { onShowGamePickerChanged(false) },
            sheetState = bottomSheetState,
            modifier = Modifier.statusBarsPadding()
        ) {
            GameList(
                gameFiles = gameFiles,
                onGameSelected = { gameFile ->
                    onGameSelected(gameFile)
                    onShowGamePickerChanged(false)
                },
                contentPadding = PaddingValues(
                    start = DolphinTheme.scaffoldPadding,
                    end = DolphinTheme.scaffoldPadding,
                    bottom = 16.dp
                ),
            )
        }
    }

    ReadOnlyTextField(
        value = game,
        label = stringResource(R.string.netplay_game_label),
        onClick = if (isHosting) {
            { onShowGamePickerChanged(true) }
        } else {
            null
        },
        modifier = Modifier.fillMaxWidth()
    )
}

@Composable
private fun GameList(
    gameFiles: List<GameFile>,
    onGameSelected: (GameFile) -> Unit,
    contentPadding: PaddingValues = PaddingValues(),
) {
    LazyVerticalGrid(
        columns = GridCells.Adaptive(minSize = 120.dp),
        contentPadding = contentPadding,
        horizontalArrangement = Arrangement.spacedBy(12.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        items(gameFiles, key = { it.getPath() }) { gameFile ->
            GameGridItem(
                gameFile = gameFile,
                onClick = { onGameSelected(gameFile) },
            )
        }
    }
}

@Composable
private fun GameGridItem(
    gameFile: GameFile,
    onClick: () -> Unit,
) {
    Card(
        onClick = onClick,
    ) {
        Column {
            AsyncImage(
                model = ImageRequest.Builder(LocalContext.current)
                    .data(gameFile)
                    .error(R.drawable.no_banner)
                    .build(),
                contentDescription = gameFile.getTitle(),
                contentScale = ContentScale.Crop,
                imageLoader = CoilUtils.imageLoader,
                modifier = Modifier
                    .fillMaxWidth()
                    .aspectRatio(0.7f)
            )
            Text(
                text = gameFile.getTitle(),
                style = MaterialTheme.typography.bodySmall,
                maxLines = 2,
                minLines = 2,
                overflow = TextOverflow.Ellipsis,
                modifier = Modifier
                    .padding(8.dp)
            )
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun JoinAddressSection(
    joinAddresses: Map<JoinInfoType, JoinAddress>,
    selectedType: JoinInfoType,
    onSelectedTypeChanged: (JoinInfoType) -> Unit,
) {
    val address = joinAddresses[selectedType] ?: joinAddresses.values.first()

    @Suppress("UnusedBoxWithConstraintsScope")
    BoxWithConstraints(modifier = Modifier.fillMaxWidth()) {
        if (maxWidth > 392.dp) {
            Row(
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                modifier = Modifier.fillMaxWidth()
            ) {
                JoinInfoDropdown(
                    joinAddresses = joinAddresses,
                    selectedType = selectedType,
                    onSelectedTypeChanged = onSelectedTypeChanged,
                    modifier = Modifier.weight(0.39f),
                )
                AddressRow(
                    joinInfoType = selectedType,
                    address = address,
                    modifier = Modifier.weight(0.61f),
                )
            }
        } else {
            Column(modifier = Modifier.fillMaxWidth()) {
                JoinInfoDropdown(
                    joinAddresses = joinAddresses,
                    selectedType = selectedType,
                    onSelectedTypeChanged = onSelectedTypeChanged,
                    modifier = Modifier.fillMaxWidth(),
                )
                MenuSpacer()
                AddressRow(
                    joinInfoType = selectedType,
                    address = address,
                    modifier = Modifier.fillMaxWidth(),
                )
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun JoinInfoDropdown(
    joinAddresses: Map<JoinInfoType, JoinAddress>,
    selectedType: JoinInfoType,
    onSelectedTypeChanged: (JoinInfoType) -> Unit,
    modifier: Modifier = Modifier,
) {
    var expanded by remember { mutableStateOf(false) }

    ExposedDropdownMenuBox(
        expanded = expanded,
        onExpandedChange = { expanded = it },
        modifier = modifier,
    ) {
        OutlinedTextField(
            value = stringResource(selectedType.labelId),
            onValueChange = {},
            readOnly = true,
            label = { Text(stringResource(R.string.netplay_host_address_label)) },
            trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded) },
            modifier = Modifier
                .menuAnchor(MenuAnchorType.PrimaryNotEditable)
                .fillMaxWidth()
        )

        ExposedDropdownMenu(
            expanded = expanded,
            onDismissRequest = { expanded = false },
        ) {
            joinAddresses.keys.forEach { type ->
                DropdownMenuItem(
                    text = { Text(stringResource(type.labelId)) },
                    onClick = {
                        onSelectedTypeChanged(type)
                        expanded = false
                    },
                    contentPadding = ExposedDropdownMenuDefaults.ItemContentPadding,
                )
            }
        }
    }
}

@Composable
private fun AddressRow(
    joinInfoType: JoinInfoType,
    address: JoinAddress,
    modifier: Modifier = Modifier,
) {
    val context = LocalContext.current

    ReadOnlyTextField(
        value = when (address) {
            is JoinAddress.Loading -> stringResource(R.string.netplay_address_loading)
            is JoinAddress.Loaded -> address.address
            is JoinAddress.Unknown -> stringResource(R.string.netplay_address_unknown)
        },
        label = stringResource(
            if (joinInfoType == JoinInfoType.ROOM_ID) R.string.netplay_code_label
            else R.string.netplay_address_label
        ),
        onClick = when (address) {
            is JoinAddress.Loaded -> {
                {
                    val intent = Intent(Intent.ACTION_SEND).apply {
                        type = "text/plain"
                        putExtra(Intent.EXTRA_TEXT, address.address)
                    }
                    context.startActivity(Intent.createChooser(intent, null))
                }
            }

            is JoinAddress.Unknown -> address.retry
            is JoinAddress.Loading -> null
        },
        textStyle = if (address is JoinAddress.Loading) {
            LocalTextStyle.current.copy(color = MaterialTheme.colorScheme.onSurfaceVariant)
        } else {
            null
        },
        trailingIcon = {
            when (address) {
                is JoinAddress.Loaded -> Icon(
                    imageVector = Icons.Filled.Share,
                    contentDescription = stringResource(R.string.netplay_address_share),
                )

                is JoinAddress.Unknown -> Icon(
                    imageVector = Icons.Filled.Refresh,
                    contentDescription = stringResource(R.string.netplay_address_retry),
                )

                is JoinAddress.Loading -> CircularProgressIndicator(
                    modifier = Modifier.size(24.dp),
                    strokeWidth = 2.dp,
                )
            }
        },
        modifier = modifier,
    )
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
        fatalTraversalError = emptyFlow(),
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
        isHosting = true,
        onStartGame = {},
        onGameSelected = {},
        gameFiles = emptyList(),
        hostInputAuthorityEnabled = true,
        maxBuffer = 10,
        onMaxBufferChanged = {},
        saveTransferProgress = null,
        gameDigestProgress = null,
        joinAddresses = mapOf(
            JoinInfoType.EXTERNAL to JoinAddress.Loaded("203.0.113.1:2626"),
            JoinInfoType.LOCAL to JoinAddress.Loaded("192.168.1.5:2626"),
        ),
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
