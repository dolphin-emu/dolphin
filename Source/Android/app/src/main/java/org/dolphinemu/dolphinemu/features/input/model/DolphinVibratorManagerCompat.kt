// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model

import android.os.Vibrator

class DolphinVibratorManagerCompat(vibrator: Vibrator) : DolphinVibratorManager {
    private val vibrator: Vibrator
    private val vibratorIds: IntArray

    init {
        this.vibrator = vibrator
        vibratorIds = if (vibrator.hasVibrator()) intArrayOf(0) else intArrayOf()
    }

    override fun getVibrator(vibratorId: Int): Vibrator {
        if (vibratorId > vibratorIds.size)
            throw IndexOutOfBoundsException()

        return vibrator
    }

    override fun getVibratorIds(): IntArray = vibratorIds
}
