// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model

import android.content.Context
import android.os.Build
import android.os.Vibrator
import android.os.VibratorManager
import android.view.InputDevice
import org.dolphinemu.dolphinemu.DolphinApplication

object DolphinVibratorManagerFactory {
    fun getDeviceVibratorManager(device: InputDevice): DolphinVibratorManager {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            DolphinVibratorManagerPassthrough(device.vibratorManager)
        } else {
            DolphinVibratorManagerCompat(device.vibrator)
        }
    }

    fun getSystemVibratorManager(): DolphinVibratorManager {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            val vibratorManager = DolphinApplication.getAppContext()
                .getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as VibratorManager?
            if (vibratorManager != null) {
                return DolphinVibratorManagerPassthrough(vibratorManager)
            }
        }
        val vibrator = DolphinApplication.getAppContext()
            .getSystemService(Context.VIBRATOR_SERVICE) as Vibrator
        return DolphinVibratorManagerCompat(vibrator)
    }
}
