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
     * with a fallback to a waveform or a legacy vibration.
     *
     * @param effect The desired [HapticEffect] of the feedback.
     * @param intensity The desired intensity of the feedback.
     * This should be an integer between [MIN_INTENSITY] and [MAX_INTENSITY].
     */
    fun provideFeedback(effect: HapticEffect, @IntRange(from = 1, to = 5) intensity: Int) {
        if (primitiveSupport) {
            vibrator.vibrate(
                VibrationEffect.startComposition()
                    .addPrimitive(getPrimitive(effect), intensity / MAX_INTENSITY.toFloat())
                    .compose()
            )
        } else {
            val timings = getTimings(effect, intensity)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                if (vibrator.hasAmplitudeControl()) {
                    vibrator.vibrate(
                        VibrationEffect.createWaveform(
                            timings, getAmplitudes(effect, intensity), -1
                        )
                    )
                } else {
                    vibrator.vibrate(VibrationEffect.createWaveform(timings, -1))
                }
            } else {
                vibrator.vibrate(timings.sum())
            }
        }
    }

    /**
     * Get the timings for a waveform vibration based on the [effect] and the [intensity].
     *
     * @param effect The [HapticEffect] of the vibration.
     * @param intensity The intensity of the vibration (from [MIN_INTENSITY] and [MAX_INTENSITY]).
     * @return The LongArray of timings for the specified [effect].
     */
    private fun getTimings(
        effect: HapticEffect, @IntRange(from = 1, to = 5) intensity: Int
    ): LongArray {
        // Note: It is recommended that these values differ by a ratio of 1.4 or more,
        // so the difference in the duration of the vibration can be easily perceived.
        // The base duration of some effects has been taken
        // from `frameworks/base/core/res/res/values/config.xml`.
        return when (effect) {
            HapticEffect.CLICK -> longArrayOf(0L, 20L * intensity)
            HapticEffect.TICK -> longArrayOf(0L, 14L * intensity)
            HapticEffect.LOW_TICK -> longArrayOf(0L, 10L * intensity)
            HapticEffect.SPIN -> SPIN_TIMINGS //TODO: Make this scalable?
        }
    }

    /**
     * Get the amplitudes for a waveform vibration based on the [effect] and the [intensity].
     *
     * @param effect The [HapticEffect] of the vibration.
     * @param intensity The intensity of the vibration (from [MIN_INTENSITY] and [MAX_INTENSITY]).
     * @return The IntArray of amplitudes for the specified [effect].
     */
    @RequiresApi(Build.VERSION_CODES.O)
    private fun getAmplitudes(
        effect: HapticEffect, @IntRange(from = 1, to = 5) intensity: Int
    ): IntArray {
        return when (effect) {
            //TODO: Make amplitudes of these effects scalable?
            HapticEffect.CLICK, HapticEffect.TICK, HapticEffect.LOW_TICK -> intArrayOf(
                0,
                VibrationEffect.DEFAULT_AMPLITUDE
            )

            HapticEffect.SPIN -> {
                val scale = intensity / MAX_INTENSITY.toFloat()
                IntArray(SPIN_AMPLITUDES.size) { (SPIN_AMPLITUDES[it] * scale).toInt() }
            }
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
        private val SPIN_TIMINGS = longArrayOf(15L, 15L, 10L, 15L, 10L, 15L, 10L, 10L)
        private val SPIN_AMPLITUDES = intArrayOf(0, 128, 255, 100, 200, 32, 64, 0)
    }
}

enum class HapticEffect {
    CLICK,
    TICK,
    LOW_TICK,
    SPIN
}
