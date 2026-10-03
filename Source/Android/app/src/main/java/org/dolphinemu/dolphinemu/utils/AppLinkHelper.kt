// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.net.Uri
import androidx.annotation.StringDef
import org.dolphinemu.dolphinemu.ui.platform.PlatformTab

/**
 * Helps link home screen selection to a game.
 */
object AppLinkHelper {
    const val PLAY = "play"
    const val BROWSE = "browse"
    private const val SCHEMA_URI_PREFIX = "dolphinemu://app/"
    private const val URI_PLAY = SCHEMA_URI_PREFIX + PLAY
    private const val URI_VIEW = SCHEMA_URI_PREFIX + BROWSE
    private const val URI_INDEX_OPTION = 0
    private const val URI_INDEX_CHANNEL = 1
    private const val URI_INDEX_GAME = 2

    @JvmStatic
    fun buildGameUri(channelId: Long, gameId: String): Uri {
        return Uri.parse(URI_PLAY).buildUpon().appendPath(channelId.toString()).appendPath(gameId)
            .build()
    }

    @JvmStatic
    fun buildBrowseUri(platformTab: PlatformTab): Uri {
        return Uri.parse(URI_VIEW).buildUpon().appendPath(platformTab.idString).build()
    }

    @JvmStatic
    fun extractAction(uri: Uri): AppLinkAction {
        if (isGameUri(uri)) {
            return PlayAction(extractChannelId(uri), extractGameId(uri))
        } else if (isBrowseUri(uri)) {
            return BrowseAction(extractSubscriptionName(uri))
        }

        throw IllegalArgumentException("No action found for uri $uri")
    }

    private fun isGameUri(uri: Uri): Boolean {
        if (uri.pathSegments.isEmpty()) {
            return false
        }
        val option = uri.pathSegments[URI_INDEX_OPTION]
        return PLAY == option
    }

    private fun isBrowseUri(uri: Uri): Boolean {
        if (uri.pathSegments.isEmpty()) {
            return false
        }

        val option = uri.pathSegments[URI_INDEX_OPTION]
        return BROWSE == option
    }

    private fun extractSubscriptionName(uri: Uri): String {
        return extract(uri, URI_INDEX_CHANNEL)!!
    }

    private fun extractChannelId(uri: Uri): Long {
        return extractLong(uri, URI_INDEX_CHANNEL)
    }

    private fun extractGameId(uri: Uri): String {
        return extract(uri, URI_INDEX_GAME)!!
    }

    private fun extractLong(uri: Uri, index: Int): Long {
        return java.lang.Long.parseLong(extract(uri, index)!!)
    }

    private fun extract(uri: Uri, index: Int): String? {
        val pathSegments = uri.pathSegments
        if (pathSegments.isEmpty() || pathSegments.size < index) {
            return null
        }
        return pathSegments[index]
    }

    @StringDef(BROWSE, PLAY)
    @Retention(AnnotationRetention.SOURCE)
    annotation class ActionFlags

    /**
     * Action for deep linking.
     */
    interface AppLinkAction {
        /**
         * Returns an string representation of the action.
         */
        @get:ActionFlags
        val action: String
    }

    /**
     * Action when clicking the channel icon
     */
    class BrowseAction(private val subscriptionName: String) : AppLinkAction {
        override val action: String
            get() = BROWSE
    }

    /**
     * Action when clicking a program (game)
     */
    class PlayAction(
        val channelId: Long, val gameId: String
    ) : AppLinkAction {
        override val action: String
            get() = PLAY
    }
}
