// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.retroachievements

import android.app.Activity
import android.app.AlertDialog
import android.os.Bundle
import kotlin.concurrent.thread
import org.dolphinemu.dolphinemu.R

/**
 * Asks the user whether one named app may set the RetroAchievements host override.
 *
 * Launch it with startActivityForResult and without FLAG_ACTIVITY_NEW_TASK. RESULT_OK means the
 * app is allowed. The asking app is identified with [getCallingPackage], which only Binder fills
 * in. A request that cannot name its caller is refused instead of guessed.
 */
class RetroAchievementsHostOverrideAccessActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val caller = callingPackage
        if (caller == null) {
            finishWith(RESULT_CANCELED)
            return
        }

        thread {
            val ready = RetroAchievementsHostOverrideAccess.awaitDirectoriesReady(this)
            runOnUiThread {
                if (isFinishing || isDestroyed) {
                    return@runOnUiThread
                }
                when {
                    !ready -> finishWith(RESULT_CANCELED)
                    RetroAchievementsHostOverrideAccess.isGranted(this, caller) ->
                        finishWith(RESULT_OK)
                    else -> showPrompt(caller)
                }
            }
        }
    }

    private fun showPrompt(caller: String) {
        val dialog = AlertDialog.Builder(this)
            .setTitle(R.string.achievements_host_override_request_title)
            .setMessage(
                getString(
                    R.string.achievements_host_override_request_message,
                    appLabel(caller),
                    caller
                )
            )
            .setPositiveButton(R.string.achievements_host_override_allow) { _, _ ->
                val granted = RetroAchievementsHostOverrideAccess.grant(this, caller)
                finishWith(if (granted) RESULT_OK else RESULT_CANCELED)
            }
            .setNegativeButton(R.string.achievements_host_override_deny) { _, _ ->
                finishWith(RESULT_CANCELED)
            }
            .setOnCancelListener { finishWith(RESULT_CANCELED) }
            .create()
        dialog.show()
        dialog.window?.decorView?.filterTouchesWhenObscured = true
    }

    private fun finishWith(result: Int) {
        setResult(result)
        finish()
    }

    private fun appLabel(packageName: String): String = runCatching {
        packageManager.getApplicationLabel(packageManager.getApplicationInfo(packageName, 0))
            .toString()
    }.getOrDefault(packageName)
}
