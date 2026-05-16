// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.content.ContentResolver
import android.net.Uri
import android.provider.DocumentsContract
import android.provider.DocumentsContract.Document
import androidx.annotation.Keep
import androidx.core.net.toUri
import org.dolphinemu.dolphinemu.DolphinApplication
import java.io.FileNotFoundException

/*
  We use a lot of "catch (Exception e)" in this class. This is for two reasons:

  1. We don't want any exceptions to escape to native code, as this leads to nasty crashes
     that often don't have stack traces that make sense.

  2. The sheer number of different exceptions, both documented and undocumented. These include:
     - FileNotFoundException when a file doesn't exist
     - FileNotFoundException when using an invalid open mode (according to the documentation)
     - IllegalArgumentException when using an invalid open mode (in practice with FileProvider)
     - IllegalArgumentException when providing a tree where a document was expected and vice versa
     - SecurityException when trying to access something the user hasn't granted us permission to
     - UnsupportedOperationException when a URI specifies a storage provider that doesn't exist
 */
object ContentHandler {
    @JvmStatic
    fun isContentUri(pathOrUri: String): Boolean {
        return pathOrUri.startsWith("content://")
    }

    @Keep
    @JvmStatic
    fun openFd(uri: String, mode: String): Int {
        try {
            return getContentResolver().openFileDescriptor(unmangle(uri), mode)!!.detachFd()
        } catch (_: SecurityException) {
            Log.error("Tried to open $uri without permission")
        } catch (_: Exception) {
        }

        return -1
    }

    @Keep
    @JvmStatic
    fun delete(uri: String): Boolean {
        try {
            return DocumentsContract.deleteDocument(getContentResolver(), unmangle(uri))
        } catch (_: FileNotFoundException) {
            // Return true because we care about the file not being there, not the actual delete.
            return true
        } catch (_: SecurityException) {
            Log.error("Tried to delete $uri without permission")
        } catch (_: Exception) {
        }

        return false
    }

    @JvmStatic
    fun exists(uri: String): Boolean {
        try {
            val documentUri = treeToDocument(unmangle(uri))
            val projection = arrayOf(Document.COLUMN_MIME_TYPE, Document.COLUMN_SIZE)
            getContentResolver().query(documentUri, projection, null, null, null)?.use { cursor ->
                return cursor.count > 0
            }
        } catch (_: SecurityException) {
            Log.error("Tried to check if $uri exists without permission")
        } catch (_: Exception) {
        }

        return false
    }

    /**
     * @return -1 if not found, -2 if directory, file size otherwise
     */
    @Keep
    @JvmStatic
    fun getSizeAndIsDirectory(uri: String): Long {
        try {
            val documentUri = treeToDocument(unmangle(uri))
            val projection = arrayOf(Document.COLUMN_MIME_TYPE, Document.COLUMN_SIZE)
            getContentResolver().query(documentUri, projection, null, null, null)?.use { cursor ->
                if (cursor.moveToFirst()) {
                    return if (Document.MIME_TYPE_DIR == cursor.getString(0)) {
                        -2
                    } else {
                        if (cursor.isNull(1)) 0 else cursor.getLong(1)
                    }
                }
            }
        } catch (_: SecurityException) {
            Log.error("Tried to get metadata for $uri without permission")
        } catch (_: Exception) {
        }

        return -1
    }

    @Keep
    @JvmStatic
    fun getDisplayName(uri: String): String? {
        try {
            return getDisplayName(unmangle(uri))
        } catch (_: Exception) {
        }

        return null
    }

    @JvmStatic
    fun getDisplayName(uri: Uri): String? {
        val projection = arrayOf(Document.COLUMN_DISPLAY_NAME)
        val documentUri = treeToDocument(uri)
        try {
            getContentResolver().query(documentUri, projection, null, null, null)?.use { cursor ->
                if (cursor.moveToFirst()) {
                    return cursor.getString(0)
                }
            }
        } catch (_: SecurityException) {
            Log.error("Tried to get display name of $uri without permission")
        } catch (_: Exception) {
        }

        return null
    }

    @Keep
    @JvmStatic
    fun getChildNames(uri: String, recursive: Boolean): Array<String> {
        try {
            return getChildNames(unmangle(uri), recursive)
        } catch (_: Exception) {
        }

        return emptyArray()
    }

    @JvmStatic
    fun getChildNames(uri: Uri, recursive: Boolean): Array<String> {
        val result = ArrayList<String>()

        if (recursive) {
            fun addChildren(documentId: String) {
                forEachChild(uri, documentId) { displayName, childDocumentId, isDirectory ->
                    if (isDirectory) {
                        addChildren(childDocumentId)
                    } else {
                        result.add(displayName)
                    }
                }
            }

            addChildren(DocumentsContract.getDocumentId(treeToDocument(uri)))
        } else {
            forEachChild(
                uri, DocumentsContract.getDocumentId(treeToDocument(uri))
            ) { displayName, _, isDirectory ->
                if (!isDirectory) {
                    result.add(displayName)
                }
            }
        }

        return result.toTypedArray()
    }

    @Keep
    @JvmStatic
    fun doFileSearch(
        directory: String, extensions: Array<String>, recursive: Boolean
    ): Array<String> {
        val result = ArrayList<String>()

        try {
            val uri = unmangle(directory)
            val documentId = DocumentsContract.getDocumentId(treeToDocument(uri))
            val acceptAll = extensions.isEmpty()
            val extensionCheck: (String) -> Boolean = { displayName ->
                val extension = FileBrowserHelper.getExtension(displayName, true)
                extension != null && extensions.any { it.equals(extension, ignoreCase = true) }
            }
            if (recursive) {
                doRecursiveFileSearch(uri, directory, documentId, result, acceptAll, extensionCheck)
            } else {
                forEachChild(uri, documentId) { displayName, _, isDirectory ->
                    val childPath = "$directory/$displayName"
                    if (acceptAll || (!isDirectory && extensionCheck(displayName))) {
                        result.add(childPath)
                    }
                }
            }
        } catch (_: Exception) {
        }

        return result.toTypedArray()
    }

    private fun doRecursiveFileSearch(
        baseUri: Uri,
        path: String,
        documentId: String,
        resultOut: MutableList<String>,
        acceptAll: Boolean,
        extensionCheck: (String) -> Boolean
    ) {
        forEachChild(baseUri, documentId) { displayName, childDocumentId, isDirectory ->
            val childPath = "$path/$displayName"
            if (acceptAll || (!isDirectory && extensionCheck(displayName))) {
                resultOut.add(childPath)
            }
            if (isDirectory) {
                doRecursiveFileSearch(
                    baseUri, childPath, childDocumentId, resultOut, acceptAll, extensionCheck
                )
            }
        }
    }

    private fun forEachChild(
        uri: Uri, documentId: String, callback: (String, String, Boolean) -> Unit
    ) {
        try {
            val childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(uri, documentId)
            val projection = arrayOf(
                Document.COLUMN_DISPLAY_NAME, Document.COLUMN_MIME_TYPE, Document.COLUMN_DOCUMENT_ID
            )
            getContentResolver().query(childrenUri, projection, null, null, null)?.use { cursor ->
                while (cursor.moveToNext()) {
                    callback(
                        cursor.getString(0),
                        cursor.getString(2),
                        Document.MIME_TYPE_DIR == cursor.getString(1)
                    )
                }
            }
        } catch (_: SecurityException) {
            Log.error("Tried to get children of $uri without permission")
        } catch (_: Exception) {
        }
    }

    @Throws(FileNotFoundException::class, SecurityException::class)
    private fun getChild(parentUri: Uri, childName: String): Uri {
        val parentId = DocumentsContract.getDocumentId(treeToDocument(parentUri))
        val childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(parentUri, parentId)

        val projection = arrayOf(Document.COLUMN_DISPLAY_NAME, Document.COLUMN_DOCUMENT_ID)
        val selection = Document.COLUMN_DISPLAY_NAME + "=?"
        val selectionArgs = arrayOf(childName)
        try {
            getContentResolver().query(childrenUri, projection, selection, selectionArgs, null)
                ?.use { cursor ->
                    while (cursor.moveToNext()) {
                        // FileProvider seemingly doesn't support selections, so we have to manually filter here
                        if (childName == cursor.getString(0)) {
                            return DocumentsContract.buildDocumentUriUsingTree(
                                parentUri, cursor.getString(1)
                            )
                        }
                    }
                }
        } catch (_: SecurityException) {
            Log.error("Tried to get child $childName of $parentUri without permission")
        } catch (_: Exception) {
        }

        throw FileNotFoundException("$parentUri/$childName")
    }

    /**
     * Since our C++ code was written under the assumption that it would be running under a filesystem
     * which supports normal paths, it appends a slash followed by a file name when it wants to access
     * a file in a directory. This function translates that into the type of URI that SAF requires.
     *
     * In order to detect whether a URI is mangled or not, we make the assumption that an
     * unmangled URI contains at least one % and does not contain any slashes after the last %.
     * This seems to hold for all common storage providers, but it is theoretically for a storage
     * provider to use URIs without any % characters.
     */
    @Throws(FileNotFoundException::class, SecurityException::class)
    @JvmStatic
    fun unmangle(uri: String): Uri {
        val lastComponentEnd = getLastComponentEnd(uri)
        val lastComponentStart = getLastComponentStart(uri, lastComponentEnd)

        return if (lastComponentStart == 0) {
            uri.substring(0, lastComponentEnd).toUri()
        } else {
            val parentUri = unmangle(uri.substring(0, lastComponentStart))
            val childName = uri.substring(lastComponentStart, lastComponentEnd)
            getChild(parentUri, childName)
        }
    }

    /**
     * Returns the last character which is not a slash.
     */
    private fun getLastComponentEnd(uri: String): Int {
        var i = uri.length
        while (i > 0 && uri[i - 1] == '/') {
            i--
        }
        return i
    }

    /**
     * Scans backwards starting from lastComponentEnd and returns the index after the first slash
     * it finds, but only if there is a % before that slash and there is no % after it.
     */
    private fun getLastComponentStart(uri: String, lastComponentEnd: Int): Int {
        var i = lastComponentEnd
        while (i > 0 && uri[i - 1] != '/') {
            i--
            if (uri[i] == '%') {
                return 0
            }
        }

        var j = i
        while (j > 0) {
            j--
            if (uri[j] == '%') {
                return i
            }
        }

        return 0
    }

    private fun treeToDocument(uri: Uri): Uri {
        return if (isTreeUri(uri)) {
            val documentId = DocumentsContract.getTreeDocumentId(uri)
            DocumentsContract.buildDocumentUriUsingTree(uri, documentId)
        } else {
            uri
        }
    }

    /**
     * This is like DocumentsContract.isTreeUri, except it doesn't return true for URIs like
     * content://com.example/tree/12/document/24/. We want to treat those as documents, not trees.
     */
    private fun isTreeUri(uri: Uri): Boolean {
        val pathSegments = uri.pathSegments
        return pathSegments.size == 2 && pathSegments[0] == "tree"
    }

    private fun getContentResolver(): ContentResolver {
        return DolphinApplication.getAppContext().contentResolver
    }
}
