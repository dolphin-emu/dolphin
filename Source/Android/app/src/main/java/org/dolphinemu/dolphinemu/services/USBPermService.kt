// SPDX-License-Identifier: GPL-2.0-or-later

@file:Suppress("DEPRECATION", "OVERRIDE_DEPRECATION")

package org.dolphinemu.dolphinemu.services

import android.app.IntentService
import android.content.Intent

class USBPermService : IntentService("USBPermService") {
    // Needed when extending IntentService.
    // We don't care about the results of the intent handler for this.
    override fun onHandleIntent(intent: Intent?) {
    }
}
