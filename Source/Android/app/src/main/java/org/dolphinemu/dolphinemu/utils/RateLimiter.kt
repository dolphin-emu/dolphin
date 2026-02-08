// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.os.Handler
import android.os.SystemClock

class RateLimiter(
    private val handler: Handler,
    private val delayBetweenRunsMs: Int,
    private val runnable: Runnable
)  {
    private var nextAllowedRun = 0L
    private var pendingRun = false

    fun run() {
        if (!pendingRun) {
            val time = SystemClock.uptimeMillis()
            if (time >= nextAllowedRun) {
                runnable.run()
                nextAllowedRun = time + delayBetweenRunsMs
            } else {
                pendingRun = true
                handler.postAtTime({
                    runnable.run()
                    nextAllowedRun = SystemClock.uptimeMillis() + delayBetweenRunsMs
                    pendingRun = false
                }, nextAllowedRun)
            }
        }
    }
}
