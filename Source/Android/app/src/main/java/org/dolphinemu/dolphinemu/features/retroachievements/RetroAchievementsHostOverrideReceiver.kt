// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.retroachievements

import android.app.Activity
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.os.SystemClock
import androidx.core.content.ContextCompat
import java.net.URI
import java.util.Locale
import java.util.concurrent.CountDownLatch
import kotlin.concurrent.thread
import org.dolphinemu.dolphinemu.features.settings.model.AchievementModel
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization

class RetroAchievementsHostOverrideReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        val action = intent.action ?: return
        val packageName = context.packageName
        val pendingResult = goAsync()

        thread {
            try {
                pendingResult.resultCode = handleAction(context, action, packageName, intent)
            } finally {
                pendingResult.finish()
            }
        }
    }

    private fun handleAction(
        context: Context,
        action: String,
        packageName: String,
        intent: Intent
    ): Int {
        if (!awaitDirectoriesReady(context)) {
            return Activity.RESULT_CANCELED
        }

        return when (action) {
            "$packageName$ACTION_SET_HOST_OVERRIDE_SUFFIX" -> {
                val hostUrl = normalizeHostUrl(intent.getStringExtra(EXTRA_HOST))
                    ?: return Activity.RESULT_CANCELED
                runOnMainThread(context) { AchievementModel.setHostOverride(hostUrl) }
                Activity.RESULT_OK
            }

            "$packageName$ACTION_CLEAR_HOST_OVERRIDE_SUFFIX" -> {
                runOnMainThread(context) { AchievementModel.clearHostOverride() }
                Activity.RESULT_OK
            }

            else -> Activity.RESULT_CANCELED
        }
    }

    private fun runOnMainThread(context: Context, block: () -> Unit) {
        val latch = CountDownLatch(1)
        ContextCompat.getMainExecutor(context).execute {
            try {
                block()
            } finally {
                latch.countDown()
            }
        }
        latch.await()
    }

    private fun awaitDirectoriesReady(context: Context): Boolean {
        if (DirectoryInitialization.areDolphinDirectoriesReady()) {
            return true
        }
        if (DirectoryInitialization.isWaitingForWriteAccess(context)) {
            return false
        }

        ContextCompat.getMainExecutor(context).execute { DirectoryInitialization.start(context) }

        val deadline = SystemClock.elapsedRealtime() + DIRECTORY_INIT_TIMEOUT_MS
        while (!DirectoryInitialization.areDolphinDirectoriesReady()) {
            if (SystemClock.elapsedRealtime() > deadline) {
                return false
            }
            Thread.sleep(DIRECTORY_INIT_POLL_MS)
        }
        return true
    }

    private fun normalizeHostUrl(value: String?): String? {
        val trimmedValue = value?.trim().orEmpty()

        if (trimmedValue.isEmpty()) {
            return null
        }

        val candidate = if ("://" in trimmedValue) trimmedValue else "http://$trimmedValue"
        val uri = runCatching { URI(candidate) }.getOrNull() ?: return null
        val host = uri.host?.lowercase(Locale.US) ?: return null

        if (uri.port !in 1..65535) {
            return null
        }

        if (!uri.rawPath.isNullOrEmpty() && uri.rawPath != "/") {
            return null
        }

        if (uri.rawQuery != null || uri.rawFragment != null || uri.userInfo != null) {
            return null
        }

        return "$host:${uri.port}"
    }

    companion object {
        private const val ACTION_SET_HOST_OVERRIDE_SUFFIX =
            ".action.SET_RETROACHIEVEMENTS_HOST_OVERRIDE"
        private const val ACTION_CLEAR_HOST_OVERRIDE_SUFFIX =
            ".action.CLEAR_RETROACHIEVEMENTS_HOST_OVERRIDE"

        private const val DIRECTORY_INIT_TIMEOUT_MS = 9000L
        private const val DIRECTORY_INIT_POLL_MS = 50L

        const val EXTRA_HOST = "host"
    }
}
