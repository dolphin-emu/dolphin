package org.dolphinemu.dolphinemu.features.netplay.model

import android.content.Context
import org.dolphinemu.dolphinemu.R

sealed class TraversalState {
    data object Connecting : TraversalState()

    data class Connected(val hostCode: String, val externalAddress: String) : TraversalState()

    data class Failure(val reason: String) : TraversalState() {
        fun message(context: Context) = when (reason) {
            "BadHost" -> context.getString(R.string.netplay_traversal_error_bad_host)
            "VersionTooOld" -> context.getString(R.string.netplay_traversal_error_version_too_old)
            else -> reason
        }
    }
}
