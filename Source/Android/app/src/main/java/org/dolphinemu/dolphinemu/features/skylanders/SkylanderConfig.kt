// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.skylanders

import android.content.Context
import android.net.Uri
import android.util.Pair
import org.dolphinemu.dolphinemu.features.skylanders.model.SkylanderPair
import org.dolphinemu.dolphinemu.utils.ContentHandler
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization
import org.dolphinemu.dolphinemu.utils.Log
import java.io.File
import java.io.FileOutputStream

object SkylanderConfig {
    var LIST_SKYLANDERS: Map<SkylanderPair, String> = getSkylanderMap()
    var REVERSE_LIST_SKYLANDERS: Map<String, SkylanderPair> = getInverseSkylanderMap()

    private external fun getSkylanderMap(): Map<SkylanderPair, String>
    private external fun getInverseSkylanderMap(): Map<String, SkylanderPair>

    @JvmStatic
    external fun removeSkylander(slot: Int): Boolean

    @JvmStatic
    external fun loadSkylander(slot: Int, fileName: String?): Pair<Int?, String?>?

    @JvmStatic
    external fun createSkylander(
        id: Int,
        variant: Int,
        fileName: String,
        slot: Int
    ): Pair<Int, String>

    /**
     * Obtains the guaranteed writable directory for Skylanders .sky files.
     * Defaults to [UserDirectory]/Load/Skylanders/ where Dolphin has full POSIX read/write access.
     */
    fun getSkylandersDirectory(context: Context? = null): File {
        val userDir = DirectoryInitialization.getUserDirectory()
        val baseDir = if (userDir.isNotBlank()) {
            File(userDir, "Load/Skylanders")
        } else if (context != null) {
            File(context.getExternalFilesDir(null) ?: context.filesDir, "Load/Skylanders")
        } else {
            File("/storage/emulated/0/Android/data/org.dolphinemu.dolphinemu/files/Load/Skylanders")
        }

        if (!baseDir.exists()) {
            baseDir.mkdirs()
        }
        return baseDir
    }

    /**
     * Sanitizes a string so it can be safely used as a filename.
     */
    fun sanitizeFileName(name: String): String {
        return name.replace(Regex("[\\\\/:*?\"<>|]"), "_").trim()
    }

    /**
     * Locates or prepares the File path for a given Skylander ID and Variant.
     * Looks for "<Name>.sky" first, then "sky_<id>_<variant>.sky".
     */
    fun getFigureFileForSkylander(context: Context?, id: Int, variant: Int): File {
        val dir = getSkylandersDirectory(context)
        val name = LIST_SKYLANDERS[SkylanderPair(id, variant)]
        if (!name.isNullOrBlank()) {
            val namedFile = File(dir, "${sanitizeFileName(name)}.sky")
            if (namedFile.exists()) {
                return namedFile
            }
        }
        val idFile = File(dir, "sky_${id}_${variant}.sky")
        if (idFile.exists()) {
            return idFile
        }
        // If neither exists, prefer named file if name is known, otherwise id file
        return if (!name.isNullOrBlank()) {
            File(dir, "${sanitizeFileName(name)}.sky")
        } else {
            idFile
        }
    }

    /**
     * Safely imports a .sky file from a SAF content:// Uri into Dolphin's local storage.
     * This avoids read-only SAF file descriptor failures and solves Issue #13956.
     */
    fun importFigureFromUri(context: Context, uri: Uri, fallbackName: String? = null): File? {
        return try {
            val displayName = ContentHandler.getDisplayName(uri.toString()) ?: fallbackName ?: "figure_${System.currentTimeMillis()}.sky"
            val targetFile = File(getSkylandersDirectory(context), sanitizeFileName(displayName))

            context.contentResolver.openInputStream(uri)?.use { input ->
                FileOutputStream(targetFile).use { output ->
                    input.copyTo(output)
                }
            }
            if (targetFile.exists() && targetFile.length() > 0) {
                targetFile
            } else {
                null
            }
        } catch (e: Exception) {
            Log.error("Failed to import Skylander file from URI: $uri - ${e.message}")
            null
        }
    }

    /**
     * Loads an existing .sky figure if available, or automatically creates a new one if it does not exist yet.
     */
    fun loadOrAutoCreate(context: Context?, id: Int, variant: Int, slot: Int): Pair<Int?, String?>? {
        val figureFile = getFigureFileForSkylander(context, id, variant)
        return if (figureFile.exists() && figureFile.length() >= 1024) {
            loadSkylander(slot, figureFile.absolutePath)
        } else {
            val created = createSkylander(id, variant, figureFile.absolutePath, slot)
            Pair(created.first, created.second)
        }
    }

    /**
     * Resolves a Skylander by its name (case-insensitive) and loads or auto-creates it.
     */
    fun loadOrAutoCreateByName(context: Context?, name: String, slot: Int): Pair<Int?, String?>? {
        val pair = REVERSE_LIST_SKYLANDERS[name]
            ?: REVERSE_LIST_SKYLANDERS.entries.firstOrNull { it.key.equals(name, ignoreCase = true) }?.value
            ?: return null

        return loadOrAutoCreate(context, pair.id, pair.variant, slot)
    }
}
