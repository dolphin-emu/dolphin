// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.retroachievements

import android.content.DialogInterface
import androidx.fragment.app.FragmentActivity
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlin.concurrent.thread
import org.dolphinemu.dolphinemu.R

/** Asks once whether to use RAOfflineProxy, when it is installed and running at game launch. */
object RetroAchievementsProxyPrompt {
    @JvmStatic
    fun run(activity: FragmentActivity, onDone: Runnable) {
        val context = activity.applicationContext
        thread {
            val ask = RetroAchievementsProxy.shouldAskToUse(context)
            activity.runOnUiThread {
                val packageName = RetroAchievementsProxy.installedProxyPackage(context)
                if (!ask || packageName == null || activity.isFinishing || activity.isDestroyed) {
                    onDone.run()
                    return@runOnUiThread
                }

                MaterialAlertDialogBuilder(activity)
                    .setTitle(R.string.achievements_proxy_prompt_title)
                    .setMessage(
                        activity.getString(
                            R.string.achievements_proxy_prompt_message,
                            appLabel(activity, packageName),
                            packageName
                        )
                    )
                    .setPositiveButton(R.string.yes) { _: DialogInterface?, _: Int ->
                        RetroAchievementsProxy.enableAndSave(context)
                        onDone.run()
                    }
                    .setNegativeButton(R.string.no) { _: DialogInterface?, _: Int ->
                        RetroAchievementsProxy.declinePromptAndSave()
                        onDone.run()
                    }
                    .setCancelable(false)
                    .show()
            }
        }
    }

    private fun appLabel(activity: FragmentActivity, packageName: String): String = runCatching {
        val packageManager = activity.packageManager
        packageManager.getApplicationLabel(packageManager.getApplicationInfo(packageName, 0)).toString()
    }.getOrDefault(packageName)
}
