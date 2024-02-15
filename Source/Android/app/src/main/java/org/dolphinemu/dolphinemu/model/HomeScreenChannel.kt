// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.model

import android.net.Uri

/**
 * Represents a home screen channel for Android TV api 26+
 */
class HomeScreenChannel(var name: String, var description: String, var appLinkIntentUri: Uri) {
    var channelId: Long = 0
}
