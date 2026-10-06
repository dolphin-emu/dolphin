// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.netplay

import android.app.Application
import android.content.Context
import android.content.pm.PackageManager
import android.net.wifi.p2p.WifiP2pManager
import android.os.Build
import androidx.annotation.RequiresFeature
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withTimeoutOrNull
import org.dolphinemu.dolphinemu.DolphinApplication
import kotlin.time.Duration.Companion.seconds

object WifiDirectManager {

    private val hasWifiDirectFeature: Boolean
        get() = DolphinApplication.instance.packageManager.hasSystemFeature(PackageManager.FEATURE_WIFI_DIRECT)

    val isWifiDirectSupported: Boolean by lazy { Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q && hasWifiDirectFeature }

    private val mutex = Mutex()

    @Volatile
    private var closeComplete: CompletableDeferred<Unit>? = null

    @Volatile
    var activeSession: WifiDirectSession? = null
        private set

    @RequiresFeature(
        name = PackageManager.FEATURE_WIFI_DIRECT,
        enforcement = "android.content.pm.PackageManager#hasSystemFeature",
    )
    suspend fun createHostSession(): WifiDirectHostSession =
        createSession { application, wifiP2pManager, onClosed ->
            WifiDirectHostSession(
                application,
                wifiP2pManager,
                onClosed
            )
        }

    @RequiresFeature(
        name = PackageManager.FEATURE_WIFI_DIRECT,
        enforcement = "android.content.pm.PackageManager#hasSystemFeature",
    )
    suspend fun createClientSession(): WifiDirectClientSession =
        createSession { application, wifiP2pManager, onClosed ->
            WifiDirectClientSession(
                application,
                wifiP2pManager,
                onClosed
            )
        }

    private suspend fun <T : WifiDirectSession> createSession(
        factory: (Application, WifiP2pManager, () -> Unit) -> T,
    ): T = mutex.withLock {
        withTimeoutOrNull(30.seconds) {
            closeComplete?.await() ?: Unit
        } ?: throw IllegalStateException("Tried to create a new WifiDirectSession while the old one was not closed or did not close in time. isClosed=${activeSession?.isClosed}")

        closeComplete = CompletableDeferred()

        val application = DolphinApplication.instance
        val wifiP2pManager =
            application.getSystemService(Context.WIFI_P2P_SERVICE) as WifiP2pManager //TODO can be null apparently
        val onClosed: () -> Unit = {
            activeSession = null
            closeComplete?.complete(Unit)
        }

        factory(
            application,
            wifiP2pManager,
            onClosed,
        ).also {
            activeSession = it
            it.clearGroupAndPeers()
        }
    }
}
