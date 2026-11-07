// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.retroachievements

import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.SystemClock
import androidx.core.content.ContextCompat
import java.security.MessageDigest
import java.util.concurrent.CountDownLatch
import org.dolphinemu.dolphinemu.features.settings.model.NativeConfig
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization

/**
 * Which apps may set the RetroAchievements host override. A grant is stored together with the
 * signing certificate of the app, so replacing the app with a different build under the same
 * package name asks the user again.
 */
object RetroAchievementsHostOverrideAccess {
    private const val ENTRY_SEPARATOR = ","
    private const val DIGEST_SEPARATOR = ":"
    private const val DIRECTORY_INIT_TIMEOUT_MS = 9000L
    private const val DIRECTORY_INIT_POLL_MS = 50L

    fun isGranted(context: Context, packageName: String): Boolean {
        val digest = signingDigest(context, packageName) ?: return false
        return parseGrants(StringSetting.ACHIEVEMENTS_HOST_OVERRIDE_APPS.string)[packageName] == digest
    }

    fun hasGrants(): Boolean = StringSetting.ACHIEVEMENTS_HOST_OVERRIDE_APPS.string.isNotEmpty()

    fun grant(context: Context, packageName: String): Boolean {
        val digest = signingDigest(context, packageName) ?: return false
        val grants = parseGrants(StringSetting.ACHIEVEMENTS_HOST_OVERRIDE_APPS.string)
        StringSetting.ACHIEVEMENTS_HOST_OVERRIDE_APPS.setString(
            NativeConfig.LAYER_BASE,
            formatGrants(grants + (packageName to digest))
        )
        NativeConfig.save(NativeConfig.LAYER_BASE)
        return true
    }

    fun revokeAll() {
        StringSetting.ACHIEVEMENTS_HOST_OVERRIDE_APPS.deleteFromLayer(NativeConfig.LAYER_BASE)
        NativeConfig.save(NativeConfig.LAYER_BASE)
    }

    internal fun parseGrants(raw: String): Map<String, String> =
        raw.split(ENTRY_SEPARATOR).mapNotNull { entry ->
            val parts = entry.split(DIGEST_SEPARATOR, limit = 2)
            if (parts.size == 2 && parts[0].isNotEmpty() && parts[1].isNotEmpty()) {
                parts[0] to parts[1]
            } else {
                null
            }
        }.toMap()

    internal fun formatGrants(grants: Map<String, String>): String =
        grants.entries.sortedBy { it.key }
            .joinToString(ENTRY_SEPARATOR) { "${it.key}$DIGEST_SEPARATOR${it.value}" }

    internal fun digestOf(certificates: List<ByteArray>): String =
        certificates
            .map { certificate ->
                MessageDigest.getInstance("SHA-256").digest(certificate)
                    .joinToString("") { "%02x".format(it) }
            }
            .sorted()
            .joinToString("+")

    private fun signingDigest(context: Context, packageName: String): String? {
        val certificates = try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                val info = context.packageManager.getPackageInfo(
                    packageName,
                    PackageManager.GET_SIGNING_CERTIFICATES
                ).signingInfo ?: return null
                info.apkContentsSigners.map { it.toByteArray() }
            } else {
                @Suppress("DEPRECATION")
                context.packageManager.getPackageInfo(
                    packageName,
                    PackageManager.GET_SIGNATURES
                ).signatures?.map { it.toByteArray() } ?: return null
            }
        } catch (_: PackageManager.NameNotFoundException) {
            return null
        }
        return certificates.takeIf { it.isNotEmpty() }?.let(::digestOf)
    }

    fun awaitDirectoriesReady(context: Context): Boolean {
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

    fun runOnMainThread(context: Context, block: () -> Unit) {
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
}
