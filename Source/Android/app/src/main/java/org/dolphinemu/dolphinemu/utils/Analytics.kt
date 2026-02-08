// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.os.Build
import androidx.annotation.Keep
import androidx.fragment.app.FragmentActivity
import com.android.volley.Response
import com.android.volley.toolbox.StringRequest
import org.dolphinemu.dolphinemu.DolphinApplication
import org.dolphinemu.dolphinemu.dialogs.AnalyticsDialog
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting
import org.dolphinemu.dolphinemu.features.settings.model.Settings

object Analytics {
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

            // Context is set to null to avoid toasts
            settings.saveSettings(null)
        }
    }

    @Keep
    @JvmStatic
    fun sendReport(endpoint: String, data: ByteArray) {
        val request: StringRequest = object : StringRequest(
            Method.POST,
            endpoint,
            null,
            Response.ErrorListener { Log.debug("Failed to send report") }) {
            override fun getBody(): ByteArray {
                return data
            }
        }
        VolleyUtil.getQueue().add(request)
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
