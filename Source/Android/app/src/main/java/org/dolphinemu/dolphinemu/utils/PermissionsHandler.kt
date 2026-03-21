// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.Manifest
import android.app.Activity
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.Environment
import androidx.core.content.ContextCompat
import androidx.fragment.app.FragmentActivity
import org.dolphinemu.dolphinemu.DolphinApplication
import org.dolphinemu.dolphinemu.NativeLibrary
import org.dolphinemu.dolphinemu.R

object PermissionsHandler {
    const val REQUEST_CODE_WRITE_PERMISSION = 500
    const val REQUEST_CODE_RECORD_AUDIO = 501

    private var writePermissionDenied = false

    @JvmStatic
    fun requestWritePermission(activity: FragmentActivity) {
        activity.requestPermissions(
            arrayOf(Manifest.permission.WRITE_EXTERNAL_STORAGE), REQUEST_CODE_WRITE_PERMISSION
        )
    }

    @JvmStatic
    fun hasWriteAccess(context: Context): Boolean {
        if (!isExternalStorageLegacy()) {
            return false
        }

        val hasWritePermission =
            ContextCompat.checkSelfPermission(context, Manifest.permission.WRITE_EXTERNAL_STORAGE)
        return hasWritePermission == PackageManager.PERMISSION_GRANTED
    }

    @JvmStatic
    fun isExternalStorageLegacy(): Boolean {
        return Build.VERSION.SDK_INT < Build.VERSION_CODES.Q || Environment.isExternalStorageLegacy()
    }

    @JvmStatic
    fun setWritePermissionDenied() {
        writePermissionDenied = true
    }

    @JvmStatic
    fun isWritePermissionDenied(): Boolean {
        return writePermissionDenied
    }

    @JvmStatic
    fun hasRecordAudioPermission(context: Context?): Boolean {
        val nonNullContext = context ?: DolphinApplication.getAppContext()
        val hasRecordPermission =
            ContextCompat.checkSelfPermission(nonNullContext, Manifest.permission.RECORD_AUDIO)
        return hasRecordPermission == PackageManager.PERMISSION_GRANTED
    }

    @JvmStatic
    fun requestRecordAudioPermission(activity: Activity?) {
        val targetActivity = activity ?: DolphinApplication.getAppActivity()!!
        if (activity == null) {
            // Calling from C++ code
            // Since the emulation (and cubeb) has already started, enabling the microphone permission
            // now might require restarting the game to be effective. Warn the user about it.
            NativeLibrary.displayAlertMsg(
                targetActivity.getString(R.string.wii_speak_permission_warning),
                targetActivity.getString(R.string.wii_speak_permission_warning_description),
                false,
                true,
                false
            )
        }

        targetActivity.requestPermissions(
            arrayOf(Manifest.permission.RECORD_AUDIO), REQUEST_CODE_RECORD_AUDIO
        )
    }
}
