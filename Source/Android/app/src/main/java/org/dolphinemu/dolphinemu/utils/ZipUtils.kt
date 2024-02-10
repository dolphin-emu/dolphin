/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package org.dolphinemu.dolphinemu.utils

import java.io.*
import java.util.zip.ZipEntry
import java.util.zip.ZipFile
import java.util.zip.ZipInputStream

interface ZipUtils {
    companion object {
        /**
         * Extracts a zip file to the given target directory.
         * @exception IOException if unzipping fails for any reason
         */
        @Throws(IOException::class)
        fun unzip(file : File, targetDirectory : File) {
            ZipFile(file).use { zipFile ->
                for (zipEntry in zipFile.entries()) {
                    val destFile = createNewFile(targetDirectory, zipEntry)
                    // If the zip entry is a file, we need to create its parent directories
                    val destDirectory : File? = if (zipEntry.isDirectory) destFile else destFile.parentFile

                    // Create the destination directory
                    if (destDirectory == null || (!destDirectory.isDirectory && !destDirectory.mkdirs()))
                        throw FileNotFoundException("Failed to create destination directory: $destDirectory")

                    // If the entry is a directory we don't need to copy anything
                    if (zipEntry.isDirectory)
                        continue

                    // Copy bytes to destination
                    try {
                        zipFile.getInputStream(zipEntry).use { inputStream ->
                            destFile.outputStream().use { outputStream ->
                                inputStream.copyTo(outputStream)
                            }
                        }
                    } catch (e : IOException) {
                        if (destFile.exists())
                            destFile.delete()
                        throw e
                    }
                }
            }
        }

        /**
         * Extracts a zip file from the given stream to the given target directory.
         *
         * This method is ~5x slower than [unzip], as [ZipInputStream] uses a fixed `512` bytes buffer for inflation,
         * instead of `8192` bytes or more used by input streams returned by [ZipFile].
         * This results in ~8x the amount of JNI calls, producing an increased number of array bounds checking, which kills performance.
         * Usage of this method is discouraged when possible, [unzip] should be used instead.
         * Nevertheless, it's the only option when extracting zips obtained from content URIs, as a [File] object cannot be obtained from them.
         * @exception IOException if unzipping fails for any reason
         */
        @Throws(IOException::class)
        fun unzip(stream : InputStream, targetDirectory : File) {
            ZipInputStream(BufferedInputStream(stream)).use { zis ->
                do {
                    // Get the next entry, break if we've reached the end
                    val zipEntry = zis.nextEntry ?: break

                    val destFile = createNewFile(targetDirectory, zipEntry)
                    // If the zip entry is a file, we need to create its parent directories
                    val destDirectory : File? = if (zipEntry.isDirectory) destFile else destFile.parentFile

                    // Create the destination directory
                    if (destDirectory == null || (!destDirectory.isDirectory && !destDirectory.mkdirs()))
                        throw FileNotFoundException("Failed to create destination directory: $destDirectory")

                    // If the entry is a directory we don't need to copy anything
                    if (zipEntry.isDirectory)
                        continue

                    // Copy bytes to destination
                    try {
                        BufferedOutputStream(destFile.outputStream()).use { zis.copyTo(it) }
                    } catch (e : IOException) {
                        if (destFile.exists())
                            destFile.delete()
                        throw e
                    }
                } while (true)
            }
        }

        /**
         * Safely creates a new destination file where the given zip entry will be extracted to.
         *
         * @exception IOException if the file was being created outside of the target directory
         * **see:** [Zip Slip](https://github.com/snyk/zip-slip-vulnerability)
         */
        @Throws(IOException::class)
        private fun createNewFile(destinationDir : File, zipEntry : ZipEntry) : File {
            val destFile = File(destinationDir, zipEntry.name)
            val destDirPath = destinationDir.canonicalPath
            val destFilePath = destFile.canonicalPath

            if (!destFilePath.startsWith(destDirPath + File.separator))
                throw IOException("Entry is outside of the target dir: " + zipEntry.name)

            return destFile
        }
    }
}
