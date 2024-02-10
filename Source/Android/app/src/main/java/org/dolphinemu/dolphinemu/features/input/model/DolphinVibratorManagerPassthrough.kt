// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model

import android.os.Build
import android.os.Vibrator
import android.os.VibratorManager
import androidx.annotation.RequiresApi

@RequiresApi(api = Build.VERSION_CODES.S)
class DolphinVibratorManagerPassthrough(private val vibratorManager: VibratorManager) :
    DolphinVibratorManager {
    override fun getVibrator(vibratorId: Int): Vibrator = vibratorManager.getVibrator(vibratorId)

    override fun getVibratorIds(): IntArray = vibratorManager.vibratorIds
}
