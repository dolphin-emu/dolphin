// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.netplay.ui

import android.content.res.Configuration
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.consumeWindowInsets
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.MediumTopAppBar
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.features.netplay.model.Player
import org.dolphinemu.dolphinemu.ui.theme.DolphinTheme
import org.dolphinemu.dolphinemu.ui.theme.MenuSpacer
import org.dolphinemu.dolphinemu.ui.theme.OutlinedBox
import org.dolphinemu.dolphinemu.ui.theme.PreviewTheme

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun NetplayScreen(
    onBackClicked: () -> Unit,
    players: List<Player>,
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
                players = players,
                modifier = modifier
            )
        } else {
            PortraitContent(
                players = players,
                modifier = modifier
            )
        }
    }
}

@Composable
private fun PortraitContent(
    players: List<Player>,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier
    ) {
        Chat(
            modifier = Modifier
                .fillMaxWidth()
                .fillMaxHeight(0.3f)
                .padding(horizontal = DolphinTheme.scaffoldPadding)
        )

        MenuSpacer()

        PLayersAndSettings(
            players = players,
            modifier = Modifier
                .weight(1f)
                .padding(horizontal = DolphinTheme.scaffoldPadding)
        )
    }
}

@Composable
private fun LandscapeContent(
    players: List<Player>,
    modifier: Modifier = Modifier,
) {
    Row(
        modifier = modifier
    ) {
        Chat(
            modifier = Modifier
                .weight(1f)
                .fillMaxHeight()
                .padding(horizontal = DolphinTheme.scaffoldPadding)
        )

        PLayersAndSettings(
            players = players,
            modifier = Modifier
                .weight(1f)
                .padding(horizontal = DolphinTheme.scaffoldPadding)
        )
    }
}

@Composable
private fun PLayersAndSettings(
    players: List<Player>,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier
            .verticalScroll(rememberScrollState())
    ) {
        PlayersTable(
            rows = buildList {
                add(listOf("Player", "Ping", "Mapping"))
                addAll(players.map { listOf(it.name, it.ping.toString(), it.mapping) })
                repeat(4 - players.size) { add(listOf("", "", "")) }
            },
            modifier = Modifier
                .fillMaxWidth()
        )
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun Chat(
    modifier: Modifier,
) {
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
            }

            Row(
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 8.dp)
            ) {
                OutlinedTextField(
                    value = "",
                    onValueChange = {},
                    modifier = Modifier
                        .weight(1f)
                )
                TextButton(
                    onClick = {},
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

@Preview
@Composable
private fun NetplayScreenPreview() {
    PreviewTheme(darkTheme = false) {
        NetplayScreen(
            onBackClicked = {},
            players = previewPlayers,
        )
    }
}

@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES)
@Composable
private fun NetplayScreenDarkPreview() {
    PreviewTheme(darkTheme = true) {
        NetplayScreen(
            onBackClicked = {},
            players = previewPlayers,
        )
    }
}

@Preview(widthDp = 891, heightDp = 411)
@Composable
private fun LandscapeNetplayScreenPreview() {
    PreviewTheme(darkTheme = false) {
        NetplayScreen(
            onBackClicked = {},
            players = previewPlayers,
        )
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
        NetplayScreen(
            onBackClicked = {},
            players = previewPlayers,
        )
    }
}

private val previewPlayers = listOf(
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
)
