// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.os.Build
import android.os.VibrationEffect
import android.os.Vibrator
import androidx.annotation.FloatRange
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
     * Perform haptic feedback by composing primitives (if supported),
     * with a fallback to a waveform or a legacy vibration.
     *
     * @param effect The [HapticEffect] of the feedback.
     * @param scale The intensity scale of the feedback.
     */
    fun provideFeedback(effect: HapticEffect, @FloatRange(from = 0.0, to = 1.0) scale: Float) {
        if (primitiveSupport) {
            vibrator.vibrate(
                VibrationEffect
                    .startComposition()
                    .addPrimitive(getPrimitive(effect), scale)
                    .compose()
            )
        } else {
            val timings = getTimings(effect, scale)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                if (vibrator.hasAmplitudeControl()) {
                    vibrator.vibrate(
                        VibrationEffect.createWaveform(
                            timings, getAmplitudes(effect, scale), -1
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
     * Get the timings for a waveform vibration based on the [effect], scaled by [scale].
     *
     * @param effect The [HapticEffect] of the vibration.
     * @param scale The intensity scale of the vibration.
     * @return The LongArray of scaled timings for the specified [effect].
     */
    private fun getTimings(
        effect: HapticEffect, @FloatRange(from = 0.0, to = 1.0) scale: Float
    ): LongArray {
        // Note: It is recommended that these values differ by a ratio of 1.4 or more,
        // so the difference in the duration of the vibration can be easily perceived.
        // Lower-end vibrators can't vibrate at all if the duration is too short.
        return when (effect) {
            HapticEffect.QUICK_FALL -> longArrayOf(0L, (100f * scale).toLong())
            HapticEffect.QUICK_RISE -> longArrayOf(0L, (70f * scale).toLong())
            HapticEffect.LOW_TICK -> longArrayOf(0L, (50f * scale).toLong())
            HapticEffect.SPIN -> LongArray(SPIN_TIMINGS.size) { (SPIN_TIMINGS[it] * scale).toLong() }
        }
    }

    /**
     * Get the amplitudes for a waveform vibration based on the [effect], scaled by [scale].
     *
     * @param effect The [HapticEffect] of the vibration.
     * @param scale The intensity scale of the vibration.
     * @return The IntArray of scaled amplitudes for the specified [effect].
     */
    @RequiresApi(Build.VERSION_CODES.O)
    private fun getAmplitudes(
        effect: HapticEffect, @FloatRange(from = 0.0, to = 1.0) scale: Float
    ): IntArray {
        // Note: It is recommended that these values differ by a ratio of 1.4 or more,
        // so the difference in the amplitude of the vibration can be easily perceived.
        return when (effect) {
            HapticEffect.QUICK_FALL -> intArrayOf(0, (180 * scale).toInt())
            HapticEffect.QUICK_RISE -> intArrayOf(0, (128 * scale).toInt())
            HapticEffect.LOW_TICK -> intArrayOf(0, (90 * scale).toInt())
            HapticEffect.SPIN -> IntArray(SPIN_AMPLITUDES.size) { (SPIN_AMPLITUDES[it] * scale).toInt() }
        }
    }

    @RequiresApi(Build.VERSION_CODES.S)
    private fun getPrimitive(effect: HapticEffect): Int {
        return when (effect) {
            HapticEffect.QUICK_FALL -> VibrationEffect.Composition.PRIMITIVE_QUICK_FALL
            HapticEffect.QUICK_RISE -> VibrationEffect.Composition.PRIMITIVE_QUICK_RISE
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
        private val SPIN_TIMINGS = longArrayOf(15L, 30L, 20L, 30L, 20L, 30L, 20L, 10L)
        private val SPIN_AMPLITUDES = intArrayOf(0, 128, 255, 100, 200, 32, 64, 0)
    }
}

enum class HapticEffect {
    QUICK_FALL,
    QUICK_RISE,
    LOW_TICK,
    SPIN
}
