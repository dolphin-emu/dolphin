// Copyright 2003 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.netplay

import androidx.annotation.Keep
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.flow.receiveAsFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.withContext
import org.dolphinemu.dolphinemu.features.netplay.model.ConnectionType

object Netplay {
    @Keep
    private var netPlayClientPointer: Long = 0

    @Keep
    private var bootSessionDataPointer: Long = 0

    val isLaunching: Boolean
        get() = bootSessionDataPointer != 0L

    private val _launchGame = Channel<String>(Channel.CONFLATED)
    val launchGame = _launchGame.receiveAsFlow()

    private val _connectionErrors = Channel<String>(Channel.BUFFERED)
    val connectionErrors = _connectionErrors.receiveAsFlow()

    @JvmStatic
    external fun isClientConnected(): Boolean

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

    suspend fun saveSetup(
        nickname: String,
        connectionType: ConnectionType,
        address: String,
        hostCode: String,
        connectPort: Int,
    ) = withContext(Dispatchers.IO) {
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

    suspend fun join(): Boolean = withContext(Dispatchers.IO) {
        netPlayClientPointer = Join()
        val isConnected = netPlayClientPointer != 0L && isClientConnected()

        if (!isActive) {
            releaseNetplayClient()
            return@withContext false
        }

        if (isConnected) {
            return@withContext true
        }

        releaseNetplayClient()
        false
    }

    private fun releaseNetplayClient() {
        if (netPlayClientPointer != 0L) {
            ReleaseNetplayClient()
            netPlayClientPointer = 0
        }
        _launchGame.flush()
        _connectionErrors.flush()
    }

    @JvmStatic
    private external fun Join(): Long

    @JvmStatic
    private external fun ReleaseNetplayClient()

    @JvmStatic
    fun onBootGame(gameFilePath: String, bootSessionDataPointer: Long) {
        this.bootSessionDataPointer = bootSessionDataPointer
        _launchGame.trySend(gameFilePath)
    }

    @JvmStatic
    fun onConnectionError(message: String) {
        _connectionErrors.trySend(message)
    }
}

private fun Channel<String>.flush() {
    while (this.tryReceive().isSuccess) Unit
}
