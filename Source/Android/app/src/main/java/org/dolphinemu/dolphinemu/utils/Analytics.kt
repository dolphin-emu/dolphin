// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.os.Build
import androidx.annotation.Keep
import androidx.fragment.app.FragmentActivity
import okhttp3.*
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.RequestBody.Companion.toRequestBody
import org.dolphinemu.dolphinemu.DolphinApplication
import org.dolphinemu.dolphinemu.dialogs.AnalyticsDialog
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting
import org.dolphinemu.dolphinemu.features.settings.model.Settings

object Analytics {
    private val client = OkHttpClient()

    private const val DEVICE_MANUFACTURER = "DEVICE_MANUFACTURER"
    private const val DEVICE_OS = "DEVICE_OS"
    private const val DEVICE_MODEL = "DEVICE_MODEL"
    private const val DEVICE_TYPE = "DEVICE_TYPE"

    @JvmStatic
    fun checkAnalyticsInit(activity: FragmentActivity) {
        AfterDirectoryInitializationRunner().runWithLifecycle(activity) {
            if (!BooleanSetting.MAIN_ANALYTICS_PERMISSION_ASKED.boolean) {
                AnalyticsDialog().show(activity.supportFragmentManager, AnalyticsDialog.TAG)
            }
        }
    }

    fun firstAnalyticsAdd(enabled: Boolean) {
        Settings().use { settings ->
            settings.loadSettings()
            BooleanSetting.MAIN_ANALYTICS_ENABLED.setBoolean(settings, enabled)
            BooleanSetting.MAIN_ANALYTICS_PERMISSION_ASKED.setBoolean(settings, true)

            settings.saveSettings()
        }
    }

    @Keep
    @JvmStatic
    fun sendReport(endpoint: String, data: ByteArray) {
        val req = Request.Builder()
            .url(endpoint)
            .post(data.toRequestBody("application/octet-stream".toMediaType()))
            .build()
        client.newCall(req).enqueue(object : Callback {
            override fun onFailure(call: Call, e: java.io.IOException) { Log.debug("Failed to send report") }
            override fun onResponse(call: Call, response: Response) { response.close() }
        })
    }

    @Keep
    @JvmStatic
    fun getValue(key: String?): String {
        return when (key) {
            DEVICE_MODEL -> Build.MODEL
            DEVICE_MANUFACTURER -> Build.MANUFACTURER
            DEVICE_OS -> Build.VERSION.SDK_INT.toString()
            DEVICE_TYPE -> if (TvUtil.isLeanback(DolphinApplication.getAppContext())) "android-tv" else "android-mobile"
            else -> ""
        }
    }
}
