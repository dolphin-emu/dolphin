// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Partially based on:
// Skyline
// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

package org.dolphinemu.dolphinemu.features

import android.annotation.TargetApi
import android.database.Cursor
import android.database.MatrixCursor
import android.os.Build
import android.os.CancellationSignal
import android.os.ParcelFileDescriptor
import android.provider.DocumentsContract
import android.provider.DocumentsProvider
import android.webkit.MimeTypeMap
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization
import java.io.File
import java.io.FileNotFoundException

@TargetApi(Build.VERSION_CODES.N)
class DocumentProvider : DocumentsProvider() {
    private var rootDirectory: File? = null

    companion object {
        public const val ROOT_ID = "root"

        private val DEFAULT_ROOT_PROJECTION = arrayOf(
            DocumentsContract.Root.COLUMN_ROOT_ID,
            DocumentsContract.Root.COLUMN_FLAGS,
            DocumentsContract.Root.COLUMN_ICON,
            DocumentsContract.Root.COLUMN_TITLE,
            DocumentsContract.Root.COLUMN_DOCUMENT_ID
        )

        private val DEFAULT_DOCUMENT_PROJECTION: Array<String> = arrayOf(
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_MIME_TYPE,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_LAST_MODIFIED,
            DocumentsContract.Document.COLUMN_FLAGS,
            DocumentsContract.Document.COLUMN_SIZE
        )
    }

    override fun onCreate(): Boolean {
        rootDirectory = DirectoryInitialization.getUserDirectoryPath(context)
        return true
    }

    override fun queryRoots(projection: Array<String>?): Cursor {
        val result = MatrixCursor(projection ?: DEFAULT_ROOT_PROJECTION)
        rootDirectory ?: return result

        result.newRow().apply {
            add(DocumentsContract.Root.COLUMN_ROOT_ID, ROOT_ID)
            add(DocumentsContract.Root.COLUMN_TITLE, context!!.getString(R.string.app_name_suffixed))
            add(DocumentsContract.Root.COLUMN_ICON, R.drawable.ic_dolphin)
            add(
                DocumentsContract.Root.COLUMN_FLAGS,
                DocumentsContract.Root.FLAG_SUPPORTS_CREATE or DocumentsContract.Root.FLAG_SUPPORTS_RECENTS or DocumentsContract.Root.FLAG_SUPPORTS_SEARCH
            )
            add(DocumentsContract.Root.COLUMN_DOCUMENT_ID, ROOT_ID)
        }

        return result
    }

    override fun queryDocument(documentId: String, projection: Array<String>?): Cursor {
        val result = MatrixCursor(projection ?: DEFAULT_DOCUMENT_PROJECTION)
        rootDirectory ?: return result
        val file = documentIdToPath(documentId)
        appendDocument(file, result)
        return result
    }

    override fun queryChildDocuments(
        parentDocumentId: String,
        projection: Array<String>?,
        queryArgs: String?
    ): Cursor {
        val result = MatrixCursor(projection ?: DEFAULT_DOCUMENT_PROJECTION)
        rootDirectory = rootDirectory ?: DirectoryInitialization.getUserDirectoryPath(context)
        rootDirectory ?: return result
        val folder = documentIdToPath(parentDocumentId)
        val files = folder.listFiles()
        if (files != null) {
            for (file in files) {
                appendDocument(file, result)
            }
        }
        return result
    }

    override fun openDocument(
        documentId: String,
        mode: String,
        signal: CancellationSignal?
    ): ParcelFileDescriptor? {
        rootDirectory ?: return null

        val file = documentIdToPath(documentId)
        return ParcelFileDescriptor.open(file, ParcelFileDescriptor.parseMode(mode))
    }

    override fun createDocument(
        parentDocumentId: String,
        mimeType: String,
        displayName: String
    ): String? {
        rootDirectory ?: return null

        val folder = documentIdToPath(parentDocumentId)
        val file = findFileNameForNewFile(File(folder, displayName))
        if (mimeType == DocumentsContract.Document.MIME_TYPE_DIR) {
            file.mkdirs()
        } else {
            file.createNewFile()
        }
        return pathToDocumentId(file)
    }

    override fun deleteDocument(documentId: String) {
        rootDirectory ?: return

        val file = documentIdToPath(documentId)
        file.deleteRecursively()
    }

    override fun renameDocument(documentId: String, displayName: String): String? {
        rootDirectory ?: return null

        val file = documentIdToPath(documentId)
        val dest = findFileNameForNewFile(File(file.parentFile, displayName))
        file.renameTo(dest)
        return pathToDocumentId(dest)
    }

    override fun isChildDocument(parentDocumentId: String, documentId: String): Boolean
        = documentId.startsWith(parentDocumentId)

    private fun appendDocument(file: File, cursor: MatrixCursor) {
        var flags = 0
        if (file.canWrite()) {
            flags = if (file.isDirectory) {
                DocumentsContract.Document.FLAG_DIR_SUPPORTS_CREATE
            } else {
                DocumentsContract.Document.FLAG_SUPPORTS_WRITE
            }
            flags = flags or DocumentsContract.Document.FLAG_SUPPORTS_DELETE
            flags = flags or DocumentsContract.Document.FLAG_SUPPORTS_RENAME
            // The system will handle copy + move for us
        }

        val name = if (file == rootDirectory) {
            context!!.getString(R.string.app_name_suffixed)
        } else {
            file.name
        }
        cursor.newRow().apply {
            add(DocumentsContract.Document.COLUMN_DOCUMENT_ID, pathToDocumentId(file))
            add(DocumentsContract.Document.COLUMN_MIME_TYPE, getTypeForFile(file))
            add(DocumentsContract.Document.COLUMN_DISPLAY_NAME, name)
            add(DocumentsContract.Document.COLUMN_LAST_MODIFIED, file.lastModified())
            add(DocumentsContract.Document.COLUMN_FLAGS, flags)
            add(DocumentsContract.Document.COLUMN_SIZE, file.length())
            if (file == rootDirectory) {
                add(DocumentsContract.Document.COLUMN_ICON, R.drawable.ic_dolphin)
            }
        }
    }

    private fun pathToDocumentId(path: File): String {
        return "$ROOT_ID/${path.toRelativeString(rootDirectory!!)}"
    }

    private fun documentIdToPath(documentId: String): File {
        val file = File(rootDirectory, documentId.substring("$ROOT_ID/".lastIndex))
        if (!file.exists()) {
            throw FileNotFoundException("File $documentId does not exist.")
        }
        return file
    }

    private fun getTypeForFile(file: File): String {
        return if (file.isDirectory)
            DocumentsContract.Document.MIME_TYPE_DIR
        else
            MimeTypeMap.getSingleton().getMimeTypeFromExtension(file.extension)
                ?: "application/octet-stream"
    }

    private fun findFileNameForNewFile(file: File): File {
        var unusedFile = file
        var i = 1
        while (unusedFile.exists()) {
            val pathWithoutExtension = unusedFile.absolutePath.substringBeforeLast('.')
            val extension = unusedFile.absolutePath.substringAfterLast('.')
            unusedFile = File("$pathWithoutExtension.$i.$extension")
            i++
        }
        return unusedFile
    }
}
