// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.netplay

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withTimeoutOrNull
import kotlin.time.Duration.Companion.seconds

object NetplayManager {

    private val mutex = Mutex()

    @Volatile
    private var closeComplete: CompletableDeferred<Unit>? = null

    @Volatile
    var activeSession: NetplaySession? = null
        private set

    suspend fun createSession(): NetplaySession = mutex.withLock {
        withTimeoutOrNull(15.seconds) {
            closeComplete?.await() ?: Unit
        } ?: throw IllegalStateException("Tried to create a new NetplaySession while the old one was not closed or did not close in time. isClosed=${activeSession?.isClosed}")

        closeComplete = CompletableDeferred()

        NetplaySession(
            onClosed = {
                activeSession = null
                closeComplete?.complete(Unit)
            }
        ).also {
            activeSession = it
        }
    }
}
