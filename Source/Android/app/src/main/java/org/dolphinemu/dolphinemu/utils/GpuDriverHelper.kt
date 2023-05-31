// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Partially based on:
// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

// Partially based on:
// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.content.Context
import android.os.Build
import kotlinx.serialization.SerializationException
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.model.GpuDriverMetadata
import java.io.File
import java.io.InputStream

private const val GPU_DRIVER_META_FILE = "meta.json"

interface GpuDriverHelper {
    companion object {
        /**
         * Returns information about the system GPU driver.
         * @return An array containing the driver vendor and the driver version, in this order, or `null` if an error occurred
         */
        private external fun getSystemDriverInfo(): Array<String>?

        /**
         * Queries the driver for custom driver loading support.
         * @return `true` if the device supports loading custom drivers, `false` otherwise
         */
        external fun supportsCustomDriverLoading(): Boolean

        /**
         * Queries the driver for manual max clock forcing support
         */
        external fun supportsForceMaxGpuClocks(): Boolean

        /**
         * Calls into the driver to force the GPU to run at the maximum possible clock speed
         * @param force Whether to enable or disable the forced clocks
         */
        external fun forceMaxGpuClocks(enable: Boolean)

        /**
         * Uninstalls the currently installed custom driver
         */
        fun uninstallDriver() {
            File(DirectoryInitialization.getExtractedDriverDirectory())
                .deleteRecursively()

            File(DirectoryInitialization.getExtractedDriverDirectory()).mkdir()
        }

        fun getInstalledDriverMetadata(): GpuDriverMetadata? {
            val metadataFile = File(
                DirectoryInitialization.getExtractedDriverDirectory(),
                GPU_DRIVER_META_FILE
            )
            if (!metadataFile.exists()) {
                return null;
            }

            return try {
                GpuDriverMetadata.deserialize(metadataFile)
            } catch (e: SerializationException) {
                null
            }
        }

        /**
         * Fetches metadata about the system driver.
         * @return A [GpuDriverMetadata] object containing data about the system driver
         */
        fun getSystemDriverMetadata(context: Context): GpuDriverMetadata? {
            val systemDriverInfo = getSystemDriverInfo()
            if (systemDriverInfo.isNullOrEmpty()) {
                return null;
            }

            return GpuDriverMetadata(
                name = context.getString(R.string.system_driver),
                author = "",
                packageVersion = "",
                vendor = systemDriverInfo[0],
                driverVersion = systemDriverInfo[1],
                minApi = 0,
                description = context.getString(R.string.system_driver_desc),
                libraryName = ""
            )
        }

        /**
         * Installs the given driver to the emulator's drivers directory.
         * @param stream InputStream of a driver package
         * @return The exit status of the installation process
         */
        fun installDriver(stream: InputStream): GpuDriverInstallResult {
            uninstallDriver()

            val driverDir = File(DirectoryInitialization.getExtractedDriverDirectory())
            try {
                ZipUtils.unzip(stream, driverDir)
            } catch (e: Exception) {
                e.printStackTrace()
                uninstallDriver()
                return GpuDriverInstallResult.InvalidArchive
            }

            // Check that the metadata file exists
            val metadataFile = File(driverDir, GPU_DRIVER_META_FILE)
            if (!metadataFile.isFile) {
                uninstallDriver()
                return GpuDriverInstallResult.MissingMetadata
            }

            // Check that the driver metadata is valid
            val driverMetadata = try {
                GpuDriverMetadata.deserialize(metadataFile)
            } catch (e: SerializationException) {
                uninstallDriver()
                return GpuDriverInstallResult.InvalidMetadata
            }

            // Check that the device satisfies the driver's minimum Android version requirements
            if (Build.VERSION.SDK_INT < driverMetadata.minApi) {
                uninstallDriver()
                return GpuDriverInstallResult.UnsupportedAndroidVersion
            }

            return GpuDriverInstallResult.Success
        }
    }
}

enum class GpuDriverInstallResult {
    Success,
    InvalidArchive,
    MissingMetadata,
    InvalidMetadata,
    UnsupportedAndroidVersion,
    AlreadyInstalled,
    FileNotFound
}
