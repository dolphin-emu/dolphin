// Copyright 2003 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.netplay

import androidx.annotation.Keep
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.channels.BufferOverflow
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.merge
import kotlinx.coroutines.flow.onEach
import kotlinx.coroutines.flow.receiveAsFlow
import kotlinx.coroutines.flow.runningFold
import kotlinx.coroutines.isActive
import kotlinx.coroutines.withContext
import org.dolphinemu.dolphinemu.features.netplay.model.GameDigestProgress
import org.dolphinemu.dolphinemu.features.netplay.model.NetplayMessage
import org.dolphinemu.dolphinemu.features.netplay.model.Player
import org.dolphinemu.dolphinemu.features.netplay.model.SaveTransferProgress

class NetplaySession(
    private val onClosed: (NetplaySession) -> Unit,
) {

    private var netPlayUICallbacksPointer: Long = nativeCreateUICallbacks()

    private var netPlayClientPointer: Long = 0

    private var netPlayServerPointer: Long = 0

    private var bootSessionDataPointer: Long = 0

    private val sessionScope = CoroutineScope(SupervisorJob())

    @Volatile
    var isClosed = false
        private set
        
    val isHosting: Boolean
        get() = netPlayServerPointer != 0L

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

    private val _desyncMessages = MutableSharedFlow<NetplayMessage.Desync>(
        extraBufferCapacity = 32,
        onBufferOverflow = BufferOverflow.DROP_OLDEST
    )
    val desyncMessages = _desyncMessages.asSharedFlow()

    private val _saveTransferProgress = MutableStateFlow<SaveTransferProgress?>(null)
    val saveTransferProgress = _saveTransferProgress.asStateFlow()

    private val _gameDigestProgress = MutableStateFlow<GameDigestProgress?>(null)
    val gameDigestProgress = _gameDigestProgress.asStateFlow()

    suspend fun join(): Boolean = withContext(Dispatchers.IO) {
        if (isClosed) throw IllegalStateException("Cannot join a closed session")

        mergeMessages()
            .runningFold(emptyList<NetplayMessage>()) { acc, msg -> listOf(msg) + acc }
            .onEach { _messages.tryEmit(it) }
            .launchIn(sessionScope)

        netPlayClientPointer = nativeJoin()

        if (netPlayClientPointer == 0L || !isActive) {
            closeBlocking()
            return@withContext false
        }

        true
    }

    suspend fun host(): Boolean = withContext(Dispatchers.IO) {
        if (isClosed) throw IllegalStateException("Cannot host a closed session")

        netPlayServerPointer = nativeHost()
        if (netPlayServerPointer == 0L || !isActive) {
            closeBlocking()
            return@withContext false
        }

        join()
    }

    fun sendMessage(message: String) = nativeSendMessage(message)

    fun adjustPadBufferSize(buffer: Int) = nativeAdjustPadBufferSize(buffer)

    fun startGame() = nativeStartGame()

    fun consumeBootSessionData(): Long {
        return bootSessionDataPointer.also {
            bootSessionDataPointer = 0
        }
    }

    suspend fun close() = withContext(Dispatchers.IO) {
        closeBlocking()
    }

    @Synchronized
    fun closeBlocking() {
        if (isClosed) return
        isClosed = true
        sessionScope.cancel()
        releaseNativeResources()
        onClosed(this)
    }

    protected fun finalize() {
        releaseNativeResources()
    }

    private fun mergeMessages(): Flow<NetplayMessage> = merge(
        chatMessages.map { NetplayMessage.Chat(it) },
        game.map { NetplayMessage.GameChanged(it) },
        hostInputAuthorityEnabled.map { NetplayMessage.HostInputAuthorityChanged(it) },
        padBuffer.map { NetplayMessage.BufferChanged(it) },
        desyncMessages,
    )

    private fun releaseNativeResources() {
        val currentBootSessionDataPointer = bootSessionDataPointer
        if (currentBootSessionDataPointer != 0L) {
            bootSessionDataPointer = 0
            nativeReleaseBootSessionData(currentBootSessionDataPointer)
        }

        val currentNetPlayClientPointer = netPlayClientPointer
        if (currentNetPlayClientPointer != 0L) {
            netPlayClientPointer = 0
            nativeReleaseClient(currentNetPlayClientPointer)
        }

        val currentNetPlayServerPointer = netPlayServerPointer
        if (currentNetPlayServerPointer != 0L) {
            netPlayServerPointer = 0
            nativeReleaseServer(currentNetPlayServerPointer)
        }

        val currentNetPlayUICallbacksPointer = netPlayUICallbacksPointer
        if (currentNetPlayUICallbacksPointer != 0L) {
            netPlayUICallbacksPointer = 0
            nativeReleaseUICallbacks(currentNetPlayUICallbacksPointer)
        }
    }

    // JNI methods

    private external fun nativeCreateUICallbacks(): Long

    private external fun nativeJoin(): Long

    private external fun nativeHost(): Long

    private external fun nativeSendMessage(message: String)

    private external fun nativeAdjustPadBufferSize(buffer: Int)

    private external fun nativeReleaseUICallbacks(pointer: Long)

    private external fun nativeReleaseClient(pointer: Long)

    private external fun nativeReleaseServer(pointer: Long)

    private external fun nativeReleaseBootSessionData(pointer: Long)

    private external fun nativeStartGame()

    // NetPlayUI callbacks

    @Keep
    fun onBootGame(gameFilePath: String, bootSessionDataPointer: Long) {
        this.bootSessionDataPointer = bootSessionDataPointer
        _stopGame.flush()
        _launchGame.trySend(gameFilePath)
    }

    @Keep
    fun onStopGame() {
        _stopGame.trySend(Unit)
    }

    @Keep
    fun onConnectionLost() {
        _connectionLost.trySend(Unit)
    }

    @Keep
    fun onConnectionError(message: String) {
        _connectionErrors.trySend(message)
    }

    @Keep
    fun onUpdate(players: Array<Player>) {
        _players.tryEmit(players.toList())
    }

    @Keep
    fun onChatMessageReceived(message: String) {
        _chatMessages.tryEmit(message)
    }

    @Keep
    fun onHostInputAuthorityChanged(enabled: Boolean) {
        _hostInputAuthorityEnabled.tryEmit(enabled)
    }

    @Keep
    fun onGameChanged(game: String) {
        _game.tryEmit(game)
    }

    @Keep
    fun onPadBufferChanged(buffer: Int) {
        if (_hostInputAuthorityEnabled.replayCache.firstOrNull() == true) return
        _padBuffer.tryEmit(buffer)
    }

    @Keep
    fun onDesync(frame: Int, player: String) {
        _desyncMessages.tryEmit(NetplayMessage.Desync(player, frame))
    }

    @Keep
    fun onShowChunkedProgressDialog(title: String, dataSize: Long, playerIds: IntArray) {
        val players = _players.replayCache.firstOrNull()
        _saveTransferProgress.value = SaveTransferProgress(
            title = title,
            totalSize = dataSize,
            playerProgresses = playerIds.map { playerId ->
                SaveTransferProgress.PlayerProgress(
                    playerId = playerId,
                    name = players?.find { it.pid == playerId }?.name ?: "Invalid Player ID",
                    progress = 0,
                )
            },
        )
    }

    @Keep
    fun onSetChunkedProgress(playerId: Int, progress: Long) {
        val current = _saveTransferProgress.value
        _saveTransferProgress.value = current?.copy(
            playerProgresses = current.playerProgresses.map {
                if (it.playerId == playerId) {
                    it.copy(progress = progress)
                } else {
                    it
                }
            }
        )
    }

    @Keep
    fun onHideChunkedProgressDialog() {
        _saveTransferProgress.value = null
    }

    @Keep
    fun onShowGameDigestDialog(title: String) {
        val players = _players.replayCache.firstOrNull()
        _gameDigestProgress.value = GameDigestProgress(
            title = title,
            playerProgresses = players?.map { player ->
                GameDigestProgress.PlayerProgress(
                    playerId = player.pid,
                    name = player.name,
                    progress = 0,
                    result = null,
                )
            } ?: emptyList(),
            matches = null,
        )
    }

    @Keep
    fun onSetGameDigestProgress(playerId: Int, progress: Int) {
        val current = _gameDigestProgress.value ?: return
        _gameDigestProgress.value = current.copy(
            playerProgresses = current.playerProgresses.map {
                if (it.playerId == playerId) it.copy(progress = progress) else it
            }
        )
    }

    @Keep
    fun onSetGameDigestResult(playerId: Int, result: String) {
        val current = _gameDigestProgress.value ?: return
        val updated = current.copy(
            playerProgresses = current.playerProgresses.map {
                if (it.playerId == playerId) it.copy(result = result) else it
            }
        )
        val finished = updated.playerProgresses.all { it.result != null }
        _gameDigestProgress.value = if (finished) {
            val results = updated.playerProgresses.map { it.result }
            updated.copy(matches = results.distinct().size == 1)
        } else {
            updated
        }
    }

    /**
     * Hosts send this when they dismiss their dialog even in a successful scenario. Ensuring
     * that the value is cleared before a new game digest is started. Without this, StateFlow
     * would not be a good choice.
     */
    @Keep
    fun onAbortGameDigest() {
        _gameDigestProgress.value = null
    }
}

private fun <T> Channel<T>.flush() {
    while (this.tryReceive().isSuccess) Unit
}
