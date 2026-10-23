// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.savemanager.ui

import android.content.res.Configuration
import android.graphics.BitmapFactory
import android.os.Bundle
import android.widget.Toast
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.compose.foundation.Image
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowDropDown
import androidx.compose.material.icons.filled.Build
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Tab
import androidx.compose.material3.TabRow
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TextField
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.VerticalDivider
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import kotlin.math.abs
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.graphics.createBitmap
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.features.savemanager.model.GBASaveFile
import org.dolphinemu.dolphinemu.features.savemanager.model.GCSaveFile
import org.dolphinemu.dolphinemu.features.savemanager.model.WiiSaveFile
import org.dolphinemu.dolphinemu.features.savemanager.ui.SaveManagerViewModel
import org.dolphinemu.dolphinemu.ui.main.ThemeProvider
import org.dolphinemu.dolphinemu.ui.theme.DolphinTheme
import org.dolphinemu.dolphinemu.utils.ContentHandler
import org.dolphinemu.dolphinemu.utils.ThemeHelper
import java.io.File
import java.io.FileOutputStream
import kotlin.time.Duration.Companion.milliseconds

class SaveManagerActivity : AppCompatActivity(), ThemeProvider {
    override var themeId: Int = 0
    private val viewModel: SaveManagerViewModel by viewModels()

    // GameCube state
    private var pendingGCImportSlot = 0
    private var pendingGCExportSlot = 0
    private var pendingGCExportIndex = 0

    // Wii state
    private var pendingWiiExportTitleId = 0L
    private var pendingWiiImportPath by mutableStateOf<String?>(null)
    private var pendingWiiImportTitle by mutableStateOf<String?>(null)

    private val gcImportPicker =
        registerForActivityResult(ActivityResultContracts.GetContent()) { uri ->
            uri?.let {
                lifecycleScope.launch {
                    if (!viewModel.importSave(pendingGCImportSlot, it.toString())) {
                        Toast.makeText(
                            this@SaveManagerActivity,
                            R.string.import_failed_generic,
                            Toast.LENGTH_SHORT
                        ).show()
                    }
                }
            }
        }

    private val gcExportPicker =
        registerForActivityResult(ActivityResultContracts.CreateDocument("application/octet-stream")) { uri ->
            uri?.let {
                lifecycleScope.launch {
                    if (!viewModel.exportSave(
                            pendingGCExportSlot,
                            pendingGCExportIndex,
                            it.toString()
                        )
                    ) {
                        Toast.makeText(
                            this@SaveManagerActivity,
                            R.string.export_failed,
                            Toast.LENGTH_SHORT
                        ).show()
                    }
                }
            }
        }

    private val wiiImportPicker =
        registerForActivityResult(ActivityResultContracts.GetContent()) { uri ->
            uri?.let {
                lifecycleScope.launch {
                    val titleId = viewModel.getImportTitleId(it.toString())
                    val existing = viewModel.wiiSaves.value.find { it.titleId == titleId }
                    if (existing != null) {
                        pendingWiiImportPath = it.toString()
                        pendingWiiImportTitle = existing.title
                    } else {
                        performWiiImport(it.toString())
                    }
                }
            }
        }

    private fun performWiiImport(path: String, overwrite: Boolean = false) {
        lifecycleScope.launch {
            if (!viewModel.importSave(path, overwrite)) {
                Toast.makeText(
                    this@SaveManagerActivity,
                    R.string.import_failed_generic,
                    Toast.LENGTH_SHORT
                ).show()
            }
            pendingWiiImportPath = null
            pendingWiiImportTitle = null
        }
    }

    private val wiiExportPicker =
        registerForActivityResult(ActivityResultContracts.CreateDocument("application/octet-stream")) { uri ->
            uri?.let {
                lifecycleScope.launch {
                    if (!viewModel.exportSave(pendingWiiExportTitleId, it.toString())) {
                        Toast.makeText(
                            this@SaveManagerActivity,
                            R.string.export_failed,
                            Toast.LENGTH_SHORT
                        ).show()
                    }
                }
            }
        }

    private var gbaImportSlotSelector: ((File) -> Unit)? = null
    private val gbaImportPicker =
        registerForActivityResult(ActivityResultContracts.GetContent()) { uri ->
            uri?.let {
                lifecycleScope.launch {
                    val displayName =
                        ContentHandler.getDisplayName(it) ?: it.lastPathSegment ?: "import.sav"
                    val tempFile = File(cacheDir, displayName)
                    contentResolver.openInputStream(it)?.use { input ->
                        FileOutputStream(tempFile).use { output -> input.copyTo(output) }
                    }
                    gbaImportSlotSelector?.invoke(tempFile)
                }
            }
        }

    private var pendingGbaExportPath: String? = null
    private val gbaExportPicker =
        registerForActivityResult(ActivityResultContracts.CreateDocument("application/octet-stream")) { uri ->
            uri?.let {
                lifecycleScope.launch {
                    pendingGbaExportPath?.let { srcPath ->
                        val srcFile = File(srcPath)
                        if (srcFile.exists()) {
                            contentResolver.openOutputStream(it)?.use { output ->
                                srcFile.inputStream().use { input -> input.copyTo(output) }
                            }
                        }
                    }
                }
            }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        ThemeHelper.setTheme(this)
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        viewModel.refreshAvailableFiles()
        viewModel.loadSaves()
        viewModel.loadGbaSaves()

        val initialTab = TAB_GAMECUBE

        setContent {
            DolphinTheme {
                SaveManagerScreen(
                    viewModel = viewModel,
                    initialTab = initialTab,
                    onGCImport = { slot ->
                        pendingGCImportSlot = slot
                        gcImportPicker.launch("*/*")
                    },
                    onGCExport = { slot, index, name ->
                        pendingGCExportSlot = slot
                        pendingGCExportIndex = index
                        gcExportPicker.launch(name)
                    },
                    onWiiImport = { wiiImportPicker.launch("*/*") },
                    onWiiExport = { titleId, name ->
                        pendingWiiExportTitleId = titleId
                        wiiExportPicker.launch(name)
                    },
                    onGbaImport = { selector ->
                        gbaImportSlotSelector = selector
                        gbaImportPicker.launch("*/*")
                    },
                    onGbaExport = { path, name ->
                        pendingGbaExportPath = path
                        gbaExportPicker.launch(name)
                    }
                )

                if (pendingWiiImportPath != null) {
                    AlertDialog(
                        onDismissRequest = {
                            pendingWiiImportPath = null
                            pendingWiiImportTitle = null
                        },
                        title = { Text(stringResource(R.string.import_save)) },
                        text = {
                            Text(
                                "A save already exists for ${pendingWiiImportTitle ?: "this game"}. Overwrite it?"
                            )
                        },
                        confirmButton = {
                            TextButton(onClick = {
                                performWiiImport(pendingWiiImportPath!!, overwrite = true)
                            }) {
                                Text(stringResource(R.string.ok))
                            }
                        },
                        dismissButton = {
                            TextButton(onClick = {
                                pendingWiiImportPath = null
                                pendingWiiImportTitle = null
                            }) {
                                Text(stringResource(R.string.cancel))
                            }
                        }
                    )
                }
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
        const val TAB_GAMECUBE = 0
        const val TAB_WII = 1
        const val TAB_GBA = 2
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SaveManagerScreen(
    viewModel: SaveManagerViewModel,
    initialTab: Int,
    onGCImport: (Int) -> Unit,
    onGCExport: (Int, Int, String) -> Unit,
    onWiiImport: () -> Unit,
    onWiiExport: (Long, String) -> Unit,
    onGbaImport: (((File) -> Unit)) -> Unit,
    onGbaExport: (String, String) -> Unit
) {
    var selectedTab by remember { mutableIntStateOf(initialTab) }
    val tabs = listOf(R.string.gamecube_submenu, R.string.wii_submenu, R.string.gba_submenu)

    var showGbaSlotDialog by remember { mutableStateOf(false) }
    var pendingGbaFile by remember { mutableStateOf<File?>(null) }
    var gbaImportOverwriteSlot by remember { mutableStateOf<Int?>(null) }
    val scope = rememberCoroutineScope()

    if (showGbaSlotDialog && pendingGbaFile != null) {
        val context = LocalContext.current
        AlertDialog(
            onDismissRequest = { showGbaSlotDialog = false },
            title = { Text(stringResource(R.string.gba_select_import_slot)) },
            text = {
                Column {
                    (1..5).forEach { slot ->
                        val label = if (slot == 5) "GBPlayer" else "Slot $slot"
                        DropdownMenuItem(
                            text = { Text(label) },
                            onClick = {
                                val nameWithoutExt = pendingGbaFile!!.nameWithoutExtension.let {
                                    if (it.length >= 3 && it[it.length - 2] == '-' && it.last() in '1'..'5') {
                                        it.substring(0, it.length - 2)
                                    } else it
                                }
                                if (viewModel.gbaSaveExists(nameWithoutExt, slot)) {
                                    gbaImportOverwriteSlot = slot
                                    showGbaSlotDialog = false
                                } else {
                                    scope.launch {
                                        if (viewModel.importGbaSave(pendingGbaFile!!, slot)) {
                                            Toast.makeText(
                                                context,
                                                "Imported to $label",
                                                Toast.LENGTH_SHORT
                                            ).show()
                                        } else {
                                            Toast.makeText(
                                                context,
                                                R.string.import_failed_generic,
                                                Toast.LENGTH_SHORT
                                            ).show()
                                        }
                                        showGbaSlotDialog = false
                                    }
                                }
                            }
                        )
                    }
                }
            },
            confirmButton = {
                TextButton(onClick = { showGbaSlotDialog = false }) {
                    Text(stringResource(R.string.cancel))
                }
            }
        )
    }

    if (gbaImportOverwriteSlot != null && pendingGbaFile != null) {
        val context = LocalContext.current
        val slot = gbaImportOverwriteSlot!!
        val label = if (slot == 5) "GBPlayer" else "Slot $slot"
        AlertDialog(
            onDismissRequest = { gbaImportOverwriteSlot = null },
            title = { Text(stringResource(R.string.import_save)) },
            text = {
                Text(stringResource(R.string.gba_slot_taken_overwrite, label))
            },
            confirmButton = {
                TextButton(onClick = {
                    scope.launch {
                        if (viewModel.importGbaSave(pendingGbaFile!!, slot)) {
                            Toast.makeText(
                                context,
                                "Imported to $label",
                                Toast.LENGTH_SHORT
                            ).show()
                        }
                        gbaImportOverwriteSlot = null
                        pendingGbaFile = null
                    }
                }) {
                    Text(stringResource(R.string.ok))
                }
            },
            dismissButton = {
                TextButton(onClick = {
                    gbaImportOverwriteSlot = null
                }) {
                    Text(stringResource(R.string.cancel))
                }
            }
        )
    }

    Scaffold(
        topBar = {
            Column {
                TopAppBar(title = { Text(stringResource(R.string.save_file_manager)) })
                TabRow(selectedTabIndex = selectedTab) {
                    tabs.forEachIndexed { index, titleRes ->
                        Tab(
                            selected = selectedTab == index,
                            onClick = { selectedTab = index },
                            text = { Text(stringResource(titleRes)) }
                        )
                    }
                }
            }
        }
    ) { paddingValues ->
        Box(Modifier.padding(paddingValues)) {
            when (selectedTab) {
                SaveManagerActivity.TAB_GAMECUBE -> GCMemcardManagerContent(
                    viewModel,
                    onGCImport,
                    onGCExport
                )

                SaveManagerActivity.TAB_WII -> WiiSaveManagerContent(
                    viewModel,
                    onWiiImport,
                    onWiiExport
                )

                SaveManagerActivity.TAB_GBA -> GBASaveManagerContent(viewModel, {
                    onGbaImport { file ->
                        pendingGbaFile = file
                        showGbaSlotDialog = true
                    }
                }, onGbaExport)
            }
        }
    }
}

@Composable
fun GCMemcardManagerContent(
    viewModel: SaveManagerViewModel,
    onImport: (Int) -> Unit,
    onExport: (Int, Int, String) -> Unit
) {
    val isLandscape = LocalConfiguration.current.orientation == Configuration.ORIENTATION_LANDSCAPE
    val modifier = Modifier.fillMaxSize()

    val slot = @Composable { num: Int, weightModifier: Modifier ->
        SlotSection(
            slotNum = num,
            viewModel = viewModel,
            onImport = { onImport(num) },
            onExport = { idx: Int, name: String -> onExport(num, idx, name) },
            modifier = weightModifier
        )
    }

    if (isLandscape) {
        Row(modifier) {
            slot(1, Modifier.weight(1f))
            VerticalDivider(thickness = 2.dp, color = Color.Gray)
            slot(2, Modifier.weight(1f))
        }
    } else {
        Column(modifier) {
            slot(1, Modifier.weight(1f))
            HorizontalDivider(thickness = 2.dp, color = Color.Gray)
            slot(2, Modifier.weight(1f))
        }
    }
}

@Composable
fun WiiSaveManagerContent(
    viewModel: SaveManagerViewModel,
    onImport: () -> Unit,
    onExport: (Long, String) -> Unit
) {
    val saves by viewModel.wiiSaves.collectAsState()

    Column(modifier = Modifier.fillMaxSize()) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 16.dp, vertical = 8.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Spacer(Modifier.weight(1f))
            Button(
                onClick = onImport,
                modifier = Modifier
                    .height(32.dp)
                    .widthIn(min = 110.dp),
                contentPadding = PaddingValues(horizontal = 8.dp),
                shape = MaterialTheme.shapes.small
            ) {
                Text(
                    stringResource(R.string.import_save),
                    maxLines = 1,
                    textAlign = TextAlign.Center,
                    style = MaterialTheme.typography.labelSmall
                )
            }
        }

        LazyColumn(
            modifier = Modifier.weight(1f),
            contentPadding = PaddingValues(8.dp),
        ) {
            items(saves, key = { it.titleId }) { save ->
                WiiSaveItem(save) { action: String ->
                    when (action) {
                        "delete" -> viewModel.deleteSave(save.titleId)
                        "export" -> onExport(save.titleId, "${save.gameId}.bin")
                    }
                }
            }
        }
    }
}

@Composable
fun SlotSection(
    slotNum: Int,
    viewModel: SaveManagerViewModel,
    onImport: () -> Unit,
    onExport: (Int, String) -> Unit,
    modifier: Modifier = Modifier
) {
    val saves by (if (slotNum == 1) viewModel.slot1Saves else viewModel.slot2Saves).collectAsState()
    val path by (if (slotNum == 1) viewModel.slot1Path else viewModel.slot2Path).collectAsState()
    val stats by (if (slotNum == 1) viewModel.slot1Stats else viewModel.slot2Stats).collectAsState()
    val availableFiles by viewModel.availableFiles.collectAsState()

    var expanded by remember { mutableStateOf(false) }
    val currentMemcard = remember(availableFiles, path) { availableFiles.find { it.path == path } }

    Column(modifier.padding(horizontal = 8.dp, vertical = 2.dp)) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Text(
                text = stringResource(if (slotNum == 1) R.string.slot_1 else R.string.slot_2),
                style = MaterialTheme.typography.titleSmall
            )
            Spacer(Modifier.width(8.dp))
            Box(Modifier.weight(1f)) {
                OutlinedButton(
                    onClick = { expanded = true },
                    modifier = Modifier.fillMaxWidth(),
                    contentPadding = PaddingValues(horizontal = 4.dp),
                    shape = MaterialTheme.shapes.small
                ) {
                    Column(Modifier.weight(1f, fill = false)) {
                        Text(
                            text = currentMemcard?.displayName ?: "Select Card",
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis,
                            style = MaterialTheme.typography.labelSmall
                        )
                        stats?.let {
                            Text(
                                text = stringResource(
                                    R.string.memcard_stats,
                                    it.freeBlocks,
                                    it.freeFiles
                                ),
                                style = MaterialTheme.typography.labelSmall.copy(
                                    fontSize = 8.sp,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant
                                )
                            )
                        }
                    }
                    Icon(Icons.Default.ArrowDropDown, null)
                }
                DropdownMenu(expanded, { expanded = false }) {
                    availableFiles.forEach { memcard ->
                        DropdownMenuItem(
                            text = { Text(memcard.displayName) },
                            onClick = {
                                viewModel.loadSaves(slotNum, memcard.path); expanded = false
                            }
                        )
                    }
                    if (path.isNotEmpty() && !File(path).isDirectory) {
                        HorizontalDivider(modifier = Modifier.padding(vertical = 4.dp))
                        DropdownMenuItem(
                            text = { Text(stringResource(R.string.fix_checksums)) },
                            leadingIcon = { Icon(Icons.Default.Build, null, Modifier.size(18.dp)) },
                            onClick = {
                                viewModel.fixChecksums(slotNum)
                                expanded = false
                            }
                        )
                    }
                }
            }

            Row(verticalAlignment = Alignment.CenterVertically) {
                Button(
                    onClick = onImport,
                    enabled = path.isNotEmpty(),
                    modifier = Modifier
                        .padding(start = 4.dp)
                        .height(32.dp)
                        .widthIn(min = 110.dp),
                    contentPadding = PaddingValues(horizontal = 8.dp),
                    shape = MaterialTheme.shapes.small
                ) {
                    Text(
                        stringResource(R.string.import_save),
                        maxLines = 1,
                        textAlign = TextAlign.Center,
                        style = MaterialTheme.typography.labelSmall
                    )
                }
            }
        }

        LazyColumn(Modifier.fillMaxSize(), contentPadding = PaddingValues(vertical = 2.dp)) {
            items(saves, key = { it.gameId + it.index }) { save ->
                SaveItem(save) { action: String ->
                    when (action) {
                        "copy" -> viewModel.copySave(slotNum, save.index)
                        "delete" -> viewModel.deleteSave(slotNum, save.index)
                        "export" -> onExport(save.index, "${save.gameId}.gci")
                    }
                }
            }
        }
    }
}

@Composable
fun SaveItem(save: GCSaveFile, onAction: (String) -> Unit) {
    var showMenu by remember { mutableStateOf(false) }

    Card(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 1.dp)
            .clickable { showMenu = true },
        shape = MaterialTheme.shapes.extraSmall,
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(
                alpha = 0.5f
            )
        )
    ) {
        Row(Modifier.padding(4.dp), verticalAlignment = Alignment.CenterVertically) {
            AnimatedGCIcon(save, Modifier.size(40.dp))
            Spacer(Modifier.width(8.dp))
            Column(Modifier.weight(1f)) {
                Text(
                    save.title,
                    style = MaterialTheme.typography.bodySmall.copy(fontSize = 12.sp),
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
                Text(
                    save.subtitle,
                    style = MaterialTheme.typography.labelSmall.copy(fontSize = 10.sp),
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
                Row(verticalAlignment = Alignment.CenterVertically) {
                    RegionFlag(save.region)
                    Spacer(Modifier.width(4.dp))
                    Text(
                        "${save.blockSize} blocks",
                        style = MaterialTheme.typography.labelSmall.copy(fontSize = 9.sp)
                    )
                }
            }
            save.banner?.let { banner ->
                val bitmap = remember(save.gameId + save.index) {
                    createBitmap(96, 32).apply { setPixels(banner, 0, 96, 0, 0, 96, 32) }
                        .asImageBitmap()
                }
                Image(
                    bitmap,
                    null,
                    Modifier.size(width = 120.dp, height = 40.dp),
                    contentScale = ContentScale.FillBounds
                )
            }
            Box {
                IconButton(onClick = { showMenu = true }, Modifier.size(28.dp)) {
                    Icon(Icons.Default.MoreVert, null, Modifier.size(16.dp))
                }
                DropdownMenu(showMenu, { showMenu = false }) {
                    DropdownMenuItem(
                        text = { Text(stringResource(R.string.copy_to_other_slot)) },
                        onClick = { onAction("copy"); showMenu = false })
                    DropdownMenuItem(
                        text = { Text(stringResource(R.string.delete_save)) },
                        onClick = { onAction("delete"); showMenu = false })
                    DropdownMenuItem(
                        text = { Text(stringResource(R.string.export_save)) },
                        onClick = { onAction("export"); showMenu = false })
                }
            }
        }
    }
}

@Composable
fun AnimatedGCIcon(save: GCSaveFile, modifier: Modifier = Modifier) {
    val frames = save.iconFrames?.takeIf { it.isNotEmpty() } ?: return
    var currentFrameIndex by remember(save.gameId + save.index) { mutableIntStateOf(0) }

    val bitmaps = remember(save.gameId + save.index) {
        frames.map { frame ->
            createBitmap(32, 32).apply { setPixels(frame, 0, 32, 0, 0, 32, 32) }.asImageBitmap()
        }
    }

    LaunchedEffect(save.gameId + save.index) {
        if (bitmaps.size > 1) {
            val msPerFrame = (when (save.iconDelay) {
                1 -> 4; 2 -> 8; 3 -> 12; else -> 4
            } * 1000L) / 60L
            while (true) {
                delay(msPerFrame.milliseconds)
                currentFrameIndex = (currentFrameIndex + 1) % bitmaps.size
            }
        }
    }

    Image(bitmaps[currentFrameIndex], null, modifier)
}

@Composable
fun WiiSaveItem(save: WiiSaveFile, onAction: (String) -> Unit) {
    var showMenu by remember { mutableStateOf(value = false) }

    Card(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp)
            .clickable { showMenu = true },
        shape = MaterialTheme.shapes.medium,
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.5f)
        )
    ) {
        Row(Modifier.padding(8.dp), verticalAlignment = Alignment.CenterVertically) {
            save.banner?.let { banner ->
                val bitmap = remember(save.titleId) {
                    createBitmap(192, 64).apply { setPixels(banner, 0, 192, 0, 0, 192, 64) }
                        .asImageBitmap()
                }
                Image(
                    bitmap,
                    null,
                    Modifier.size(width = 96.dp, height = 32.dp),
                    contentScale = ContentScale.FillBounds
                )
            } ?: Box(Modifier.size(width = 96.dp, height = 32.dp))

            Spacer(Modifier.width(12.dp))

            Column(Modifier.weight(1f)) {
                Text(
                    save.title,
                    style = MaterialTheme.typography.bodyMedium,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
                Row(verticalAlignment = Alignment.CenterVertically) {
                    RegionFlag(save.region)
                    Spacer(Modifier.width(4.dp))
                    Text(
                        save.gameId,
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            }

            Box {
                IconButton(onClick = { showMenu = true }) {
                    Icon(Icons.Default.MoreVert, null)
                }
                DropdownMenu(showMenu, { showMenu = false }) {
                    DropdownMenuItem(
                        text = { Text(stringResource(R.string.delete_save)) },
                        onClick = { onAction("delete"); showMenu = false }
                    )
                    DropdownMenuItem(
                        text = { Text(stringResource(R.string.export_save)) },
                        onClick = { onAction("export"); showMenu = false }
                    )
                }
            }
        }
    }
}

@Composable
fun RegionFlag(region: Int) {
    val flagName = when (region) {
        0 -> "Flag_Japan.png"; 1 -> "Flag_USA.png"; 2 -> "Flag_Europe.png"; else -> null
    } ?: return
    val context = LocalContext.current
    val bitmap = remember(flagName) {
        runCatching {
            context.assets.open("Sys/Resources/$flagName").use { BitmapFactory.decodeStream(it) }
        }.getOrNull()
    }?.asImageBitmap() ?: return
    Image(bitmap, null, Modifier.height(10.dp), contentScale = ContentScale.FillHeight)
}

@Composable
fun GBASaveManagerContent(
    viewModel: SaveManagerViewModel,
    onImport: () -> Unit,
    onExport: (String, String) -> Unit
) {
    val saves by viewModel.gbaSaves.collectAsState()
    var showRenameDialog by remember { mutableStateOf<GBASaveFile?>(null) }
    var showSlotDialog by remember { mutableStateOf<GBASaveFile?>(null) }
    var gbaSlotOverwriteTarget by remember { mutableStateOf<Pair<GBASaveFile, Int>?>(null) }
    var showDeleteConfirmation by remember { mutableStateOf<GBASaveFile?>(null) }

    val tintColors = remember {
        listOf(
            Color(0x44FF8A80), // Red
            Color(0x44FF80AB), // Pink
            Color(0x44EA80FC), // Purple
            Color(0x44B388FF), // Deep Purple
            Color(0x448C9EFF), // Indigo
            Color(0x4482B1FF), // Blue
            Color(0x4480D8FF), // Light Blue
            Color(0x4484FFFF), // Cyan
            Color(0x44A7FFEB), // Teal
            Color(0x44B9F6CA), // Green
            Color(0x44CCFF90), // Light Green
            Color(0x44F4FF81), // Lime
            Color(0x44FFFF8D), // Yellow
            Color(0x44FFE57F), // Amber
            Color(0x44FFD180), // Orange
            Color(0x44FF9E80)  // Deep Orange
        )
    }

    val gameColors = remember(saves) {
        saves.map { it.gameName }.distinct().associateWith { gameName ->
            tintColors[abs(gameName.hashCode()) % tintColors.size]
        }
    }

    if (gbaSlotOverwriteTarget != null) {
        val (save, slot) = gbaSlotOverwriteTarget!!
        val label = if (slot == 5) "GBPlayer" else "Slot $slot"
        AlertDialog(
            onDismissRequest = { gbaSlotOverwriteTarget = null },
            title = { Text("Change Slot") },
            text = {
                Text(stringResource(R.string.gba_slot_taken_overwrite, label))
            },
            confirmButton = {
                TextButton(onClick = {
                    viewModel.changeGbaSaveSlot(save, slot)
                    gbaSlotOverwriteTarget = null
                }) {
                    Text(stringResource(R.string.ok))
                }
            },
            dismissButton = {
                TextButton(onClick = { gbaSlotOverwriteTarget = null }) {
                    Text(stringResource(R.string.cancel))
                }
            }
        )
    }

    if (showDeleteConfirmation != null) {
        AlertDialog(
            onDismissRequest = { showDeleteConfirmation = null },
            title = { Text(stringResource(R.string.delete_save)) },
            text = {
                Text(
                    stringResource(
                        R.string.cheats_delete_confirmation,
                        showDeleteConfirmation!!.gameName
                    )
                )
            },
            confirmButton = {
                TextButton(onClick = {
                    viewModel.deleteGbaSave(showDeleteConfirmation!!.filePath)
                    showDeleteConfirmation = null
                }) {
                    Text(stringResource(R.string.delete_save))
                }
            },
            dismissButton = {
                TextButton(onClick = { showDeleteConfirmation = null }) {
                    Text(stringResource(R.string.cancel))
                }
            }
        )
    }

    if (showRenameDialog != null) {
        var newName by remember { mutableStateOf(showRenameDialog!!.gameName) }
        AlertDialog(
            onDismissRequest = { showRenameDialog = null },
            title = { Text(stringResource(R.string.cheats_edit)) },
            text = {
                TextField(
                    value = newName,
                    onValueChange = { newName = it },
                    singleLine = true
                )
            },
            confirmButton = {
                TextButton(onClick = {
                    viewModel.renameGbaSave(showRenameDialog!!, newName)
                    showRenameDialog = null
                }) {
                    Text(stringResource(R.string.ok))
                }
            },
            dismissButton = {
                TextButton(onClick = { showRenameDialog = null }) {
                    Text(stringResource(R.string.cancel))
                }
            }
        )
    }

    if (showSlotDialog != null) {
        AlertDialog(
            onDismissRequest = { showSlotDialog = null },
            title = { Text(stringResource(R.string.gba_change_slot)) },
            text = {
                Column {
                    (1..5).forEach { slot ->
                        val label = if (slot == 5) "GBPlayer" else "Slot $slot"
                        DropdownMenuItem(
                            text = { Text(label) },
                            onClick = {
                                if (viewModel.gbaSaveExists(showSlotDialog!!.gameName, slot)) {
                                    gbaSlotOverwriteTarget = showSlotDialog!! to slot
                                } else {
                                    viewModel.changeGbaSaveSlot(showSlotDialog!!, slot)
                                }
                                showSlotDialog = null
                            }
                        )
                    }
                }
            },
            confirmButton = {
                TextButton(onClick = { showSlotDialog = null }) {
                    Text(stringResource(R.string.cancel))
                }
            }
        )
    }

    Column(modifier = Modifier.fillMaxSize()) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 16.dp, vertical = 8.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Spacer(Modifier.weight(1f))
            Button(
                onClick = onImport,
                modifier = Modifier
                    .height(32.dp)
                    .widthIn(min = 110.dp),
                contentPadding = PaddingValues(horizontal = 8.dp),
                shape = MaterialTheme.shapes.small
            ) {
                Text(
                    stringResource(R.string.import_save),
                    maxLines = 1,
                    textAlign = TextAlign.Center,
                    style = MaterialTheme.typography.labelSmall
                )
            }
        }

        LazyColumn(
            modifier = Modifier.weight(1f),
            contentPadding = PaddingValues(8.dp),
        ) {
            items(saves, key = { it.filePath }) { save ->
                val tintColor = gameColors[save.gameName] ?: Color.Transparent
                GBASaveItem(save, tintColor) { action ->
                    when (action) {
                        "delete" -> showDeleteConfirmation = save
                        "export" -> onExport(save.filePath, "${save.gameName}.sav")
                        "rename" -> showRenameDialog = save
                        "slot" -> showSlotDialog = save
                    }
                }
            }
        }
    }
}

@Composable
fun GBASaveItem(save: GBASaveFile, tintColor: Color, onAction: (String) -> Unit) {
    var showMenu by remember { mutableStateOf(false) }
    val dateFormat = remember { SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.getDefault()) }
    val dateText = remember(save.lastModified) { dateFormat.format(Date(save.lastModified)) }

    Card(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp)
            .clickable { showMenu = true },
        shape = MaterialTheme.shapes.medium,
        colors = CardDefaults.cardColors(
            containerColor = if (tintColor == Color.Transparent)
                MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.5f)
            else
                tintColor
        )
    ) {
        Row(Modifier.padding(8.dp), verticalAlignment = Alignment.CenterVertically) {
            Box(
                modifier = Modifier.size(40.dp),
                contentAlignment = Alignment.Center
            ) {
                Text(
                    text = "GBA",
                    style = MaterialTheme.typography.labelLarge.copy(
                        fontWeight = FontWeight.Bold,
                        color = MaterialTheme.colorScheme.primary
                    )
                )
            }

            Spacer(Modifier.width(12.dp))

            Column(Modifier.weight(1f)) {
                Text(
                    save.gameName,
                    style = MaterialTheme.typography.bodyMedium,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
                Row(verticalAlignment = Alignment.CenterVertically) {
                    val slotText = if (save.slot == 5) "GBPlayer" else "Slot ${save.slot}"
                    Card(
                        colors = CardDefaults.cardColors(
                            containerColor = MaterialTheme.colorScheme.secondaryContainer
                        ),
                        shape = MaterialTheme.shapes.extraSmall
                    ) {
                        Text(
                            text = slotText,
                            modifier = Modifier.padding(horizontal = 4.dp, vertical = 2.dp),
                            style = MaterialTheme.typography.labelSmall,
                            color = MaterialTheme.colorScheme.onSecondaryContainer
                        )
                    }
                    Spacer(Modifier.width(8.dp))
                    Text(
                        text = dateText,
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            }

            Box {
                IconButton(onClick = { showMenu = true }) {
                    Icon(Icons.Default.MoreVert, null)
                }
                DropdownMenu(showMenu, { showMenu = false }) {
                    DropdownMenuItem(
                        text = { Text(stringResource(R.string.export_save)) },
                        onClick = { onAction("export"); showMenu = false }
                    )
                    DropdownMenuItem(
                        text = { Text(stringResource(R.string.cheats_edit)) },
                        onClick = { onAction("rename"); showMenu = false }
                    )
                    DropdownMenuItem(
                        text = { Text("Change Slot") },
                        onClick = { onAction("slot"); showMenu = false }
                    )
                    DropdownMenuItem(
                        text = { Text(stringResource(R.string.delete_save)) },
                        onClick = { onAction("delete"); showMenu = false }
                    )
                }
            }
        }
    }
}
