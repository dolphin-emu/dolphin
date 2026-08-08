// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.services

import androidx.core.content.edit
import androidx.preference.PreferenceManager
import org.dolphinemu.dolphinemu.DolphinApplication
import org.dolphinemu.dolphinemu.model.GbaGameFile
import org.dolphinemu.dolphinemu.utils.ContentHandler
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper
import org.json.JSONArray
import org.json.JSONException
import java.io.File

object GbaGameFileCache {
    private const val CACHE_KEY = "GbaGameListCacheV1"

    fun load(): Array<GbaGameFile>? {
        val serialized = preferences.getString(CACHE_KEY, null) ?: return null
        return try {
            val entries = JSONArray(serialized)
            Array(entries.length()) { index ->
                val entry = entries.getJSONArray(index)
                GbaGameFile(entry.getString(0), entry.getString(1))
            }
        } catch (_: JSONException) {
            null
        }
    }

    fun scan(folderPaths: Array<String>, recursiveScan: Boolean): Array<GbaGameFile> =
        folderPaths
            .flatMap { folderPath ->
                findPaths(folderPath, recursiveScan).map { GbaGameFile(it, folderPath) }
            }
            .sortedBy { it.title.lowercase() }
            .toTypedArray()

    fun save(gameFiles: Array<GbaGameFile>) {
        val entries = JSONArray()
        gameFiles.forEach {
            entries.put(JSONArray().put(it.path).put(it.rootPath))
        }
        preferences.edit { putString(CACHE_KEY, entries.toString()) }
    }

    private fun findPaths(folderPath: String, recursiveScan: Boolean): List<String> {
        if (ContentHandler.isContentUri(folderPath)) {
            val extensions = FileBrowserHelper.GBA_ROM_EXTENSIONS.map { ".$it" }.toTypedArray()
            return ContentHandler.doFileSearch(folderPath, extensions, recursiveScan).toList()
        }

        val folder = File(folderPath)
        val files = if (recursiveScan) {
            folder.walkTopDown().asSequence()
        } else {
            folder.listFiles()?.asSequence() ?: emptySequence()
        }
        return files
            .filter {
                it.isFile && it.extension.lowercase() in FileBrowserHelper.GBA_ROM_EXTENSIONS
            }
            .map { it.absolutePath }
            .toList()
    }

    private val preferences
        get() = PreferenceManager.getDefaultSharedPreferences(DolphinApplication.getAppContext())
}
