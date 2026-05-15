// Copyright 2003 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.netplay

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock

object NetplayManager {

    private val mutex = Mutex()

    @Volatile
    private var closeComplete: CompletableDeferred<Unit>? = null

    @Volatile
    var activeSession: NetplaySession? = null
        private set

    suspend fun createSession(): NetplaySession = mutex.withLock {
        closeComplete?.await()

        // Sessions should be closed by UI navigation, but just in case.
        activeSession?.closeBlocking()

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
