// Copyright 2003 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.netplay

import androidx.annotation.Keep
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.channels.BufferOverflow
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.merge
import kotlinx.coroutines.flow.onEach
import kotlinx.coroutines.flow.receiveAsFlow
import kotlinx.coroutines.flow.runningFold
import kotlinx.coroutines.isActive
import kotlinx.coroutines.withContext
import org.dolphinemu.dolphinemu.features.netplay.model.ConnectionType
import org.dolphinemu.dolphinemu.features.netplay.model.NetplayMessage
import org.dolphinemu.dolphinemu.features.netplay.model.Player

object Netplay {
    @Keep
    private var netPlayClientPointer: Long = 0

    @Keep
    private var bootSessionDataPointer: Long = 0

    private var sessionScope: CoroutineScope? = null

    val isLaunching: Boolean
        get() = bootSessionDataPointer != 0L

    private val _launchGame = Channel<String>(Channel.CONFLATED)
    val launchGame = _launchGame.receiveAsFlow()

    private val _stopGame = Channel<Unit>(Channel.CONFLATED)
    val stopGame = _stopGame.receiveAsFlow()

    private val _connectionLost = Channel<Unit>(Channel.CONFLATED)
    val connectionLost = _connectionLost.receiveAsFlow()

    private val _connectionErrors = Channel<String>(Channel.BUFFERED)
    val connectionErrors = _connectionErrors.receiveAsFlow()

    private val _messages = MutableSharedFlow<List<NetplayMessage>>(
        replay = 1,
        onBufferOverflow = BufferOverflow.DROP_OLDEST
    )
    val messages = _messages.asSharedFlow()

    private val _players = MutableSharedFlow<List<Player>>(
        replay = 1,
        onBufferOverflow = BufferOverflow.DROP_OLDEST
    )
    val players = _players.asSharedFlow().distinctUntilChanged()

    private val _chatMessages = MutableSharedFlow<String>(
        extraBufferCapacity = 32,
        onBufferOverflow = BufferOverflow.DROP_OLDEST
    )
    val chatMessages = _chatMessages.asSharedFlow()

    private val _game = MutableSharedFlow<String>(
        replay = 1,
        onBufferOverflow = BufferOverflow.DROP_OLDEST
    )
    val game = _game.asSharedFlow()

    private val _hostInputAuthorityEnabled = MutableSharedFlow<Boolean>(
        replay = 1,
        onBufferOverflow = BufferOverflow.DROP_OLDEST
    )
    val hostInputAuthorityEnabled = _hostInputAuthorityEnabled.asSharedFlow()

    private val _padBuffer = MutableSharedFlow<Int>(
        replay = 1,
        onBufferOverflow = BufferOverflow.DROP_OLDEST
    )
    val padBuffer = _padBuffer.asSharedFlow()

    suspend fun join(): Boolean = withContext(Dispatchers.IO) {
        val scope = createSessionScope()

        // Gather all messages that should appear in the chat window.
        mergeMessages()
            .runningFold(emptyList<NetplayMessage>()) { acc, msg -> listOf(msg) + acc }
            .onEach { _messages.tryEmit(it) }
            .launchIn(scope)

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

    suspend fun quit() = withContext(Dispatchers.IO) {
        releaseNetplayClient()
    }

    @OptIn(ExperimentalCoroutinesApi::class)
    private fun releaseNetplayClient() {
        sessionScope?.cancel()
        sessionScope = null

        if (bootSessionDataPointer != 0L) {
            ReleaseBootSessionData()
            bootSessionDataPointer = 0
        }

        if (netPlayClientPointer != 0L) {
            ReleaseNetplayClient()
            netPlayClientPointer = 0
        }

        _launchGame.flush()
        _stopGame.flush()
        _connectionErrors.flush()
        _players.resetReplayCache()
        _messages.resetReplayCache()
        _chatMessages.resetReplayCache()
        _game.resetReplayCache()
        _hostInputAuthorityEnabled.resetReplayCache()
        _padBuffer.resetReplayCache()
    }

    private fun createSessionScope(): CoroutineScope {
        sessionScope?.cancel()
        return CoroutineScope(SupervisorJob() + Dispatchers.IO).also {
            sessionScope = it
        }
    }

    @JvmStatic
    private external fun Join(): Long

    @JvmStatic
    external fun isClientConnected(): Boolean

    @JvmStatic
    external fun sendMessage(message: String)

    @JvmStatic
    external fun adjustPadBufferSize(buffer: Int)

    @JvmStatic
    private external fun ReleaseBootSessionData()

    @JvmStatic
    private external fun ReleaseNetplayClient()

    private fun mergeMessages(): Flow<NetplayMessage> = merge(
        chatMessages.map { NetplayMessage.Chat(it) },
        game.map { NetplayMessage.GameChanged(it) },
        hostInputAuthorityEnabled.map { NetplayMessage.HostInputAuthorityChanged(it) },
        padBuffer.map { NetplayMessage.BufferChanged(it) },
    )

    // NetPlayUI callbacks

    @Keep
    @JvmStatic
    fun onBootGame(gameFilePath: String, bootSessionDataPointer: Long) {
        this.bootSessionDataPointer = bootSessionDataPointer
        _stopGame.flush()
        _launchGame.trySend(gameFilePath)
    }

    @Keep
    @JvmStatic
    fun onStopGame() {
        _stopGame.trySend(Unit)
    }

    @Keep
    @JvmStatic
    fun onConnectionLost() {
        _connectionLost.trySend(Unit)
    }

    @Keep
    @JvmStatic
    fun onConnectionError(message: String) {
        _connectionErrors.trySend(message)
    }

    @Keep
    @JvmStatic
    fun onUpdate(players: Array<Player>) {
        _players.tryEmit(players.toList())
    }

    @Keep
    @JvmStatic
    fun onChatMessageReceived(message: String) {
        _chatMessages.tryEmit(message)
    }

    @Keep
    @JvmStatic
    fun onHostInputAuthorityChanged(enabled: Boolean) {
        _hostInputAuthorityEnabled.tryEmit(enabled)
    }

    @Keep
    @JvmStatic
    fun onGameChanged(game: String) {
        _game.tryEmit(game)
    }

    @Keep
    @JvmStatic
    fun onPadBufferChanged(buffer: Int) {
        // Only for remote pad buffer settings. Ignore local max buffer changes.
        if (_hostInputAuthorityEnabled.replayCache.firstOrNull() == true) return
        _padBuffer.tryEmit(buffer)
    }

    // Settings
    object Settings {
        @JvmStatic
        external fun getNickname(): String

        @JvmStatic
        external fun setNickname(nickname: String)

        fun getConnectionType(): ConnectionType = ConnectionType.all
            .find { it.configValue == getTraversalChoice() } ?: throw IllegalStateException()

        @JvmStatic
        external fun getTraversalChoice(): String

        @JvmStatic
        external fun setTraversalChoice(traversalChoice: String)

        @JvmStatic
        external fun getAddress(): String

        @JvmStatic
        external fun setAddress(address: String)

        @JvmStatic
        external fun getHostCode(): String

        @JvmStatic
        external fun setHostCode(hostCode: String)

        @JvmStatic
        external fun getConnectPort(): Int

        @JvmStatic
        external fun setConnectPort(port: Int)

        @JvmStatic
        external fun getClientBufferSize(): Int

        @JvmStatic
        external fun setClientBufferSize(buffer: Int)
    }
}

private fun <T> Channel<T>.flush() {
    while (this.tryReceive().isSuccess) Unit
}
