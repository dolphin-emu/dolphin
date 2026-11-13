// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.retroachievements

import android.content.ContentProvider
import android.content.ContentValues
import android.database.Cursor
import android.net.Uri
import android.os.Bundle
import java.net.URI
import java.util.Locale
import org.dolphinemu.dolphinemu.features.settings.model.AchievementModel

/**
 * Lets a companion app point the RetroAchievements client at a local proxy.
 *
 * The provider has to be exported without a permission to be reachable at all, so access is
 * decided here, against the calling package that Binder reports and the caller cannot set. A
 * caller is only served after the user allowed it in [RetroAchievementsHostOverrideAccessActivity].
 */
class RetroAchievementsHostOverrideProvider : ContentProvider() {
    override fun onCreate(): Boolean = true

    override fun call(method: String, arg: String?, extras: Bundle?): Bundle {
        val context = context ?: return status(STATUS_UNAVAILABLE)
        val caller = callingPackage ?: return status(STATUS_DENIED)
        if (method != METHOD_SET_HOST_OVERRIDE && method != METHOD_CLEAR_HOST_OVERRIDE) {
            return status(STATUS_UNKNOWN_METHOD)
        }
        if (!RetroAchievementsHostOverrideAccess.awaitDirectoriesReady(context)) {
            return status(STATUS_UNAVAILABLE)
        }
        if (!RetroAchievementsHostOverrideAccess.isGranted(context, caller)) {
            return status(STATUS_DENIED)
        }

        if (method == METHOD_SET_HOST_OVERRIDE) {
            val hostUrl = normalizeHostUrl(arg) ?: return status(STATUS_INVALID_HOST)
            RetroAchievementsHostOverrideAccess.runOnMainThread(context) {
                AchievementModel.setHostOverride(hostUrl)
            }
        } else {
            RetroAchievementsHostOverrideAccess.runOnMainThread(context) {
                AchievementModel.clearHostOverride()
            }
        }
        return status(STATUS_OK)
    }

    override fun query(
        uri: Uri,
        projection: Array<out String>?,
        selection: String?,
        selectionArgs: Array<out String>?,
        sortOrder: String?
    ): Cursor? = null

    override fun getType(uri: Uri): String? = null

    override fun insert(uri: Uri, values: ContentValues?): Uri? = null

    override fun delete(uri: Uri, selection: String?, selectionArgs: Array<out String>?): Int = 0

    override fun update(
        uri: Uri,
        values: ContentValues?,
        selection: String?,
        selectionArgs: Array<out String>?
    ): Int = 0

    private fun status(status: String) = Bundle().apply { putString(RESULT_STATUS, status) }

    companion object {
        const val METHOD_SET_HOST_OVERRIDE = "set_host_override"
        const val METHOD_CLEAR_HOST_OVERRIDE = "clear_host_override"

        const val RESULT_STATUS = "status"
        const val STATUS_OK = "ok"
        const val STATUS_DENIED = "denied"
        const val STATUS_INVALID_HOST = "invalid_host"
        const val STATUS_UNAVAILABLE = "unavailable"
        const val STATUS_UNKNOWN_METHOD = "unknown_method"

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
    }
}
