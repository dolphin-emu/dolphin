// Copyright 2003 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.netplay

import androidx.annotation.Keep
import org.dolphinemu.dolphinemu.features.netplay.model.ConnectionType

object Netplay {
    @Keep
    private var netPlayClientPointer: Long = 0

    @JvmStatic
    external fun getNickname(): String

    fun getConnectionType(): ConnectionType = ConnectionType.all
        .find { it.configValue == getTraversalChoice() } ?: throw IllegalStateException()

    @JvmStatic
    external fun getTraversalChoice(): String

    @JvmStatic
    external fun getAddress(): String

    @JvmStatic
    external fun getHostCode(): String

    @JvmStatic
    external fun getConnectPort(): Int

    @JvmStatic
    external fun getHostPort(): Int

    @JvmStatic
    external fun getUseUpnp(): Boolean

    @JvmStatic
    external fun getEnableChunkedUploadLimit(): Boolean

    @JvmStatic
    external fun getChunkedUploadLimit(): Int

    @JvmStatic
    external fun getUseIndex(): Boolean

    @JvmStatic
    external fun getIndexRegion(): String

    @JvmStatic
    external fun getIndexName(): String

    @JvmStatic
    external fun getIndexPassword(): String

    fun saveSetup(
        nickname: String,
        connectionType: ConnectionType,
        address: String,
        hostCode: String,
        connectPort: Int,
    ) {
        SaveSetup(
            nickname = nickname,
            traversalChoice = connectionType.configValue,
            address = address,
            hostCode = hostCode,
            connectPort = connectPort,
            hostPort = 2626,
            useUpnp = false,
            useListenPort = false,
            listenPort = 2626,
            enableChunkedUploadLimit = false,
            chunkedUploadLimit = 3000,
            useIndex = false,
            indexRegion = "",
            indexName = "",
            indexPassword = "",
        )
    }

    @JvmStatic
    external fun SaveSetup(
        nickname: String,
        traversalChoice: String,
        address: String,
        hostCode: String,
        connectPort: Int,
        hostPort: Int,
        useUpnp: Boolean,
        useListenPort: Boolean,
        listenPort: Int,
        enableChunkedUploadLimit: Boolean,
        chunkedUploadLimit: Int,
        useIndex: Boolean,
        indexRegion: String,
        indexName: String,
        indexPassword: String,
    )

    fun join() {
        netPlayClientPointer = Join()
    }

    @JvmStatic
    private external fun Join(): Long
}
