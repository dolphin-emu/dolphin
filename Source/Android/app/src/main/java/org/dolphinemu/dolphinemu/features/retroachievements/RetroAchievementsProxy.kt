// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.retroachievements

import android.content.Context
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import java.net.URI
import java.security.MessageDigest
import java.util.Locale
import java.util.concurrent.Callable
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting
import org.dolphinemu.dolphinemu.features.settings.model.NativeConfig
import org.dolphinemu.dolphinemu.features.settings.model.Settings
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting
import org.json.JSONObject

/**
 * Lets the user send RetroAchievements traffic through RAOfflineProxy, a local proxy app.
 *
 * Nothing is stored about the proxy's host. When a game starts, Dolphin asks the proxy app
 * whether it is running and uses its host for that session only. The user chooses the app once;
 * the choice is pinned to the app's package and signing certificate.
 */
object RetroAchievementsProxy {
    const val AUTHORITY = "com.raofflineproxy.config"

    private const val METHOD_STATUS = "status"
    private const val RESULT_STATUS = "status"
    private const val QUERY_TIMEOUT_SECONDS = 3L
    private const val PIN_SEPARATOR = ":"

    fun isEnabled(context: Context): Boolean = hasValidPin(context)

    /** A choice was saved, but the proxy app has since been replaced by one signed differently. */
    private fun isPinStale(context: Context): Boolean =
        StringSetting.ACHIEVEMENTS_PROXY_APP.string.isNotEmpty() &&
            installedProxyPackage(context) != null &&
            !hasValidPin(context)

    private fun hasValidPin(context: Context): Boolean {
        val pin = parsePin(StringSetting.ACHIEVEMENTS_PROXY_APP.string) ?: return false
        return installedProxyPackage(context) == pin.first &&
            signingDigest(context, pin.first) == pin.second
    }

    fun installedProxyPackage(context: Context): String? =
        context.packageManager.resolveContentProvider(AUTHORITY, 0)?.packageName

    fun enable(context: Context, settings: Settings): Boolean {
        val pin = currentPin(context) ?: return false
        StringSetting.ACHIEVEMENTS_PROXY_APP.setString(settings, pin)
        return true
    }

    fun enableAndSave(context: Context): Boolean {
        val pin = currentPin(context) ?: return false
        StringSetting.ACHIEVEMENTS_PROXY_APP.setString(NativeConfig.LAYER_BASE, pin)
        NativeConfig.save(NativeConfig.LAYER_BASE)
        return true
    }

    fun declinePromptAndSave() {
        StringSetting.ACHIEVEMENTS_PROXY_APP.deleteFromLayer(NativeConfig.LAYER_BASE)
        BooleanSetting.ACHIEVEMENTS_PROXY_PROMPT_DECLINED.setBoolean(NativeConfig.LAYER_BASE, true)
        NativeConfig.save(NativeConfig.LAYER_BASE)
    }

    fun disable(settings: Settings) {
        StringSetting.ACHIEVEMENTS_PROXY_APP.delete(settings)
        BooleanSetting.ACHIEVEMENTS_PROXY_PROMPT_DECLINED.setBoolean(settings, true)
        clearSessionOverride()
    }

    /**
     * True when the proxy app is installed and running, and the user has not decided yet or the
     * app they chose was replaced by a differently signed one.
     */
    fun shouldAskToUse(context: Context): Boolean {
        val choiceIsCurrent = isEnabled(context) || BooleanSetting.ACHIEVEMENTS_PROXY_PROMPT_DECLINED.boolean
        if (choiceIsCurrent && !isPinStale(context)) return false
        if (!BooleanSetting.ACHIEVEMENTS_ENABLED.boolean) return false
        return runCatching {
            installedProxyPackage(context) != null &&
                parseHost(queryStatus(context) ?: return false) != null
        }.getOrDefault(false)
    }

    private fun currentPin(context: Context): String? {
        val packageName = installedProxyPackage(context) ?: return null
        val digest = signingDigest(context, packageName) ?: return null
        return "$packageName$PIN_SEPARATOR$digest"
    }

    /** Call before a game boots. Never throws: a broken proxy must not stop a game from starting. */
    fun applyForSession(context: Context) {
        val host = runCatching { queryHost(context) }.getOrNull()
        if (host == null) {
            clearSessionOverride()
        } else {
            StringSetting.ACHIEVEMENTS_HOST_URL.setString(NativeConfig.LAYER_CURRENT, host)
            BooleanSetting.ACHIEVEMENTS_HARDCORE_ENABLED.setBoolean(NativeConfig.LAYER_CURRENT, false)
        }
    }

    private fun clearSessionOverride() {
        StringSetting.ACHIEVEMENTS_HOST_URL.deleteFromLayer(NativeConfig.LAYER_CURRENT)
        BooleanSetting.ACHIEVEMENTS_HARDCORE_ENABLED.deleteFromLayer(NativeConfig.LAYER_CURRENT)
    }

    private fun queryHost(context: Context): String? {
        if (!hasValidPin(context)) return null

        return parseHost(queryStatus(context) ?: return null)
    }

    private fun queryStatus(context: Context): String? = callWithTimeout {
        context.contentResolver
            .call(Uri.parse("content://$AUTHORITY"), METHOD_STATUS, null, null)
            ?.getString(RESULT_STATUS)
    }

    private fun <T> callWithTimeout(block: () -> T?): T? {
        val executor = Executors.newSingleThreadExecutor()
        return try {
            executor.submit(Callable { block() }).get(QUERY_TIMEOUT_SECONDS, TimeUnit.SECONDS)
        } catch (_: Exception) {
            null
        } finally {
            executor.shutdownNow()
        }
    }

    internal fun parsePin(value: String): Pair<String, String>? {
        val parts = value.split(PIN_SEPARATOR, limit = 2)
        return if (parts.size == 2 && parts[0].isNotEmpty() && parts[1].isNotEmpty()) {
            parts[0] to parts[1]
        } else {
            null
        }
    }

    internal fun parseHost(statusJson: String): String? {
        val status = runCatching { JSONObject(statusJson) }.getOrNull() ?: return null
        if (!status.optBoolean("running", false)) {
            return null
        }
        return normalizeHostUrl(status.optString("host"))
    }

    internal fun normalizeHostUrl(value: String?): String? {
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
}
