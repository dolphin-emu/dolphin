// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.os.Build
import android.os.VibrationEffect
import android.os.Vibrator
import androidx.annotation.IntRange
import androidx.annotation.RequiresApi
import org.dolphinemu.dolphinemu.features.input.model.DolphinVibratorManagerFactory

/**
 * This class provides methods that facilitate performing haptic feedback.
 *
 * @property vibrator The [Vibrator] instance to be used for vibration.
 * Defaults to the system default vibrator.
 */
class HapticsProvider(
    private val vibrator: Vibrator =
        DolphinVibratorManagerFactory.getSystemVibratorManager().getDefaultVibrator()
) {
    private val primitiveSupport: Boolean = areAllPrimitivesSupported()

    /**
     * Perform haptic feedback by composing primitives if supported,
     * with a fallback to a waveform or a deprecated vibration.
     *
     * @param effect The desired [HapticEffect] of the feedback.
     * @param intensity The desired intensity of the feedback.
     * This should be an integer between [MIN_INTENSITY] and [MAX_INTENSITY].
     */
    fun provideFeedback(effect: HapticEffect, @IntRange(from = 1, to = 5) intensity: Int) {
        val scale = intensity / MAX_INTENSITY.toFloat()
        if (primitiveSupport) {
            vibrator.vibrate(
                VibrationEffect.startComposition().addPrimitive(getPrimitive(effect), scale)
                    .compose()
            )
        } else {
            val duration = (getMaxDuration(effect) * scale).toLong()
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                // TODO: Define amplitudes for each effect for devices with amplitude control.
                vibrator.vibrate(VibrationEffect.createWaveform(longArrayOf(0L, duration), -1))
            } else {
                vibrator.vibrate(duration)
            }
        }
    }

    /**
     * Get the maximum duration value for a fallback vibration based on the [effect].
     *
     * Most of these durations are about 5 times longer than those recommended by the Android
     * source docs, which specifies them in `frameworks/base/core/res/res/values/config.xml`.
     * That is because lower-end devices are unable to vibrate for relatively short durations.
     *
     * @param effect The [HapticEffect] of the vibration.
     * @return The maximum duration in milliseconds of the specified [effect].
     */
    private fun getMaxDuration(effect: HapticEffect): Long {
        // Note: It is recommended that these values differ by a ratio of 1.4 or more,
        // so the difference in the duration of the vibration can be easily perceived.
        return when (effect) {
            HapticEffect.CLICK -> 100L
            HapticEffect.TICK -> 70L
            HapticEffect.LOW_TICK -> 50L
            HapticEffect.SPIN -> 35L // TODO: Replace this with a "spinning" pattern?
        }
    }

    @RequiresApi(Build.VERSION_CODES.S)
    private fun getPrimitive(effect: HapticEffect): Int {
        return when (effect) {
            HapticEffect.CLICK -> VibrationEffect.Composition.PRIMITIVE_CLICK
            HapticEffect.TICK -> VibrationEffect.Composition.PRIMITIVE_TICK
            HapticEffect.LOW_TICK -> VibrationEffect.Composition.PRIMITIVE_LOW_TICK
            HapticEffect.SPIN -> VibrationEffect.Composition.PRIMITIVE_SPIN
        }
    }

    private fun areAllPrimitivesSupported(): Boolean {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.S && vibrator.areAllPrimitivesSupported(
            *HapticEffect.values().map { getPrimitive(it) }.toIntArray()
        )
    }

    companion object {
        const val MIN_INTENSITY = 1
        const val MAX_INTENSITY = 5
    }
}

enum class HapticEffect {
    CLICK,
    TICK,
    LOW_TICK,
    SPIN
}
