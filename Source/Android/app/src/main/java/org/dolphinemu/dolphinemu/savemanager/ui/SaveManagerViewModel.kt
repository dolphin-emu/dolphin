// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.savemanager.ui

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.dolphinemu.dolphinemu.NativeLibrary
import org.dolphinemu.dolphinemu.features.savemanager.model.GBASaveFile
import org.dolphinemu.dolphinemu.features.savemanager.model.GCMemcardStats
import org.dolphinemu.dolphinemu.features.savemanager.model.GCSaveFile
import org.dolphinemu.dolphinemu.features.savemanager.model.SaveManagerNative
import org.dolphinemu.dolphinemu.features.savemanager.model.WiiSaveFile
import java.io.File

data class MemcardInfo(val path: String, val displayName: String)

class SaveManagerViewModel : ViewModel() {
    // GameCube Memcard State
    private val _slot1Saves = MutableStateFlow<List<GCSaveFile>>(emptyList())
    val slot1Saves: StateFlow<List<GCSaveFile>> = _slot1Saves

    private val _slot2Saves = MutableStateFlow<List<GCSaveFile>>(emptyList())
    val slot2Saves: StateFlow<List<GCSaveFile>> = _slot2Saves

    private val _slot1Stats = MutableStateFlow<GCMemcardStats?>(null)
    val slot1Stats: StateFlow<GCMemcardStats?> = _slot1Stats

    private val _slot2Stats = MutableStateFlow<GCMemcardStats?>(null)
    val slot2Stats: StateFlow<GCMemcardStats?> = _slot2Stats

    private val _slot1Path = MutableStateFlow("")
    val slot1Path: StateFlow<String> = _slot1Path

    private val _slot2Path = MutableStateFlow("")
    val slot2Path: StateFlow<String> = _slot2Path

    private val _availableFiles = MutableStateFlow<List<MemcardInfo>>(emptyList())
    val availableFiles: StateFlow<List<MemcardInfo>> = _availableFiles

    // Wii State
    private val _wiiSaves = MutableStateFlow<List<WiiSaveFile>>(emptyList())
    val wiiSaves: StateFlow<List<WiiSaveFile>> = _wiiSaves

    // GBA State
    private val _gbaSaves = MutableStateFlow<List<GBASaveFile>>(emptyList())
    val gbaSaves: StateFlow<List<GBASaveFile>> = _gbaSaves

    // GameCube Memcard Methods
    fun refreshAvailableFiles() {
        val gcDir = File(NativeLibrary.GetUserDirectory(), "GC")

        val priorityFolders = listOf(
            "EUR/Card A", "EUR/Card B",
            "USA/Card A", "USA/Card B",
            "JAP/Card A", "JAP/Card B"
        )

        val files = mutableListOf<MemcardInfo>()

        // Add priority folders
        priorityFolders.forEach { relativePath ->
            val folder = File(gcDir, relativePath)
            files.add(MemcardInfo(folder.absolutePath, relativePath))
        }

        // Scan for .raw files
        val rawFiles = mutableListOf<MemcardInfo>()
        gcDir.listFiles()?.forEach { file ->
            if (file.isFile && file.extension.lowercase() == "raw") {
                rawFiles.add(MemcardInfo(file.absolutePath, file.name))
            }
        }
        files.addAll(rawFiles.sortedBy { it.displayName })

        _availableFiles.value = files
    }

    fun loadSaves(slot: Int, path: String) {
        val saves = SaveManagerNative.getGCSaveFiles(path).toList()
        val stats = SaveManagerNative.getGCMemcardStats(path)
        if (slot == 1) {
            _slot1Path.value = path
            _slot1Saves.value = saves
            _slot1Stats.value = stats
        } else {
            _slot2Path.value = path
            _slot2Saves.value = saves
            _slot2Stats.value = stats
        }
    }

    fun copySave(srcSlot: Int, saveIndex: Int) {
        viewModelScope.launch {
            val srcPath = if (srcSlot == 1) _slot1Path.value else _slot2Path.value
            val dstPath = if (srcSlot == 1) _slot2Path.value else _slot1Path.value

            if (srcPath.isEmpty() || dstPath.isEmpty()) return@launch

            if (withContext(Dispatchers.IO) {
                    SaveManagerNative.copyGCSaveFile(
                        srcPath,
                        saveIndex,
                        dstPath
                    )
                }) {
                loadSaves(1, _slot1Path.value)
                loadSaves(2, _slot2Path.value)
            }
        }
    }

    fun deleteSave(slot: Int, saveIndex: Int) {
        viewModelScope.launch {
            val path = if (slot == 1) _slot1Path.value else _slot2Path.value
            if (path.isEmpty()) return@launch

            if (withContext(Dispatchers.IO) {
                    SaveManagerNative.deleteGCSaveFile(
                        path,
                        saveIndex
                    )
                }) {
                loadSaves(slot, path)
            }
        }
    }

    suspend fun exportSave(slot: Int, saveIndex: Int, dstPath: String): Boolean =
        withContext(Dispatchers.IO) {
            val srcPath = if (slot == 1) _slot1Path.value else _slot2Path.value
            if (srcPath.isEmpty()) return@withContext false

            SaveManagerNative.exportGCSaveFile(srcPath, saveIndex, dstPath)
        }

    suspend fun importSave(slot: Int, srcPath: String): Boolean = withContext(Dispatchers.IO) {
        val dstPath = if (slot == 1) _slot1Path.value else _slot2Path.value
        if (dstPath.isEmpty()) return@withContext false

        if (SaveManagerNative.importGCSaveFile(srcPath, dstPath)) {
            loadSaves(slot, dstPath)
            true
        } else {
            false
        }
    }

    fun fixChecksums(slot: Int) {
        viewModelScope.launch {
            val path = if (slot == 1) _slot1Path.value else _slot2Path.value
            if (path.isEmpty()) return@launch

            if (withContext(Dispatchers.IO) { SaveManagerNative.fixGCChecksums(path) }) {
                loadSaves(slot, path)
            }
        }
    }

    // Wii Methods
    fun loadSaves() {
        viewModelScope.launch {
            val loadedSaves = withContext(Dispatchers.IO) {
                SaveManagerNative.getWiiSaveFiles().toList()
            }
            _wiiSaves.value = loadedSaves
        }
    }

    fun deleteSave(titleId: Long) {
        viewModelScope.launch {
            val success = withContext(Dispatchers.IO) {
                SaveManagerNative.deleteWiiSaveFile(titleId)
            }
            if (success) {
                loadSaves()
            }
        }
    }

    suspend fun exportSave(titleId: Long, dstPath: String): Boolean = withContext(Dispatchers.IO) {
        SaveManagerNative.exportWiiSaveFile(titleId, dstPath) == 0
    }

    suspend fun importSave(srcPath: String, overwrite: Boolean = false): Boolean =
        withContext(Dispatchers.IO) {
            val result = SaveManagerNative.importWiiSaveFile(srcPath, overwrite)
            if (result == 0) {
                loadSaves()
                true
            } else {
                false
            }
        }

    fun getImportTitleId(srcPath: String): Long {
        return SaveManagerNative.getWiiSaveTitleId(srcPath)
    }

    // GBA Methods
    fun loadGbaSaves() {
        viewModelScope.launch {
            val gbaDir = File(NativeLibrary.GetUserDirectory(), "GBA/Saves")
            val saves =
                gbaDir.listFiles()?.filter { it.extension.lowercase() == "sav" }?.map { file ->
                    val name = file.nameWithoutExtension
                    var slot = 1
                    var gameName = name
                    if (name.length >= 3 && name[name.length - 2] == '-' && name.last().isDigit()) {
                        val digit = name.last().toString().toInt()
                        if (digit in 1..5) {
                            slot = digit
                            gameName = name.substring(0, name.length - 2)
                        }
                    }
                    GBASaveFile(file.name, file.absolutePath, slot, gameName, file.lastModified())
                }?.sortedWith(compareBy({ it.gameName }, { it.slot })) ?: emptyList()
            _gbaSaves.value = saves
        }
    }

    fun gbaSaveExists(gameName: String, slot: Int): Boolean {
        val gbaDir = File(NativeLibrary.GetUserDirectory(), "GBA/Saves")
        return File(gbaDir, "$gameName-$slot.sav").exists()
    }

    fun deleteGbaSave(filePath: String) {
        viewModelScope.launch {
            val file = File(filePath)
            if (file.exists() && withContext(Dispatchers.IO) { file.delete() }) {
                loadGbaSaves()
            }
        }
    }

    suspend fun importGbaSave(srcFile: File, slot: Int): Boolean = withContext(Dispatchers.IO) {
        val gbaDir = File(NativeLibrary.GetUserDirectory(), "GBA/Saves")
        if (!gbaDir.exists()) gbaDir.mkdirs()

        var nameWithoutExt = srcFile.nameWithoutExtension
        // If the file already has a slot suffix strip it before re-appending
        if (nameWithoutExt.length >= 3 && nameWithoutExt[nameWithoutExt.length - 2] == '-') {
            val lastChar = nameWithoutExt.last()
            if (lastChar in '1'..'5') {
                nameWithoutExt = nameWithoutExt.substring(0, nameWithoutExt.length - 2)
            }
        }

        val destFileName = "$nameWithoutExt-$slot.sav"
        val destFile = File(gbaDir, destFileName)

        try {
            srcFile.inputStream().use { input ->
                destFile.outputStream().use { output ->
                    input.copyTo(output)
                }
            }
            loadGbaSaves()
            true
        } catch (e: Exception) {
            false
        }
    }

    fun renameGbaSave(save: GBASaveFile, newName: String) {
        viewModelScope.launch {
            val file = File(save.filePath)
            if (!file.exists()) return@launch

            val gbaDir = file.parentFile ?: return@launch
            val newFile = File(gbaDir, "$newName-${save.slot}.sav")

            if (withContext(Dispatchers.IO) { file.renameTo(newFile) }) {
                loadGbaSaves()
            }
        }
    }

    fun changeGbaSaveSlot(save: GBASaveFile, newSlot: Int) {
        viewModelScope.launch {
            val file = File(save.filePath)
            if (!file.exists()) return@launch

            val gbaDir = file.parentFile ?: return@launch
            val newFile = File(gbaDir, "${save.gameName}-$newSlot.sav")

            if (newFile.absolutePath == file.absolutePath) return@launch

            if (withContext(Dispatchers.IO) {
                    if (newFile.exists()) newFile.delete()
                    file.renameTo(newFile)
                }) {
                loadGbaSaves()
            }
        }
    }
}
