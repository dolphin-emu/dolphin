// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model

import android.os.Vibrator
import androidx.annotation.Keep

/**
 * A wrapper around [android.os.VibratorManager], for backwards compatibility.
 */
@Keep
interface DolphinVibratorManager {
    fun getVibrator(vibratorId: Int): Vibrator

    fun getVibratorIds(): IntArray

    fun getDefaultVibrator(): Vibrator
}
