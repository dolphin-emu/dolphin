// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.os.Build
import android.os.VibrationEffect
import android.os.Vibrator
import android.view.HapticFeedbackConstants
import android.view.View
import androidx.annotation.FloatRange
import androidx.annotation.RequiresApi
import org.dolphinemu.dolphinemu.features.input.model.DolphinVibratorManagerFactory

/**
 * Provides haptic feedback to the user.
 *
 * @property vibrator The [Vibrator] instance to be used for vibration.
 * Defaults to the system default vibrator.
 */
class HapticsProvider(
    private val vibrator: Vibrator =
        DolphinVibratorManagerFactory.getSystemVibratorManager().getDefaultVibrator()
) {
    private val primitiveSupport: Boolean = areAllPrimitivesSupported()
    private val releaseHapticFeedbackConstant =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1) {
            HapticFeedbackConstants.VIRTUAL_KEY_RELEASE
        } else {
            HapticFeedbackConstants.VIRTUAL_KEY // Same as press feedback is better than no feedback
        }

    /**
     * Perform haptic feedback natively (if a [View] is provided),
     * or by composing primitives (if supported), falling back to a waveform or a legacy vibration.
     *
     * @param effect The [HapticEffect] of the feedback.
     * @param view The [View] to perform the feedback on, can be null to vibrate using the [vibrator].
     * @param scale The intensity scale of the feedback, will only be used if [view] is not set.
     */
    fun provideFeedback(
        effect: HapticEffect,
        view: View? = null,
        @FloatRange(from = 0.0, to = 1.0) scale: Float = 0.5f
    ) {
        if (view != null) {
            view.performHapticFeedback(getHapticFeedbackConstant(effect))
        } else if (primitiveSupport) {
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
            HapticEffect.PRESS -> longArrayOf(0L, (100f * scale).toLong())
            HapticEffect.RELEASE -> longArrayOf(0L, (70f * scale).toLong())
            HapticEffect.JOYSTICK -> longArrayOf(0L, (50f * scale).toLong())
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
        // Value range is between 0 and 255 (VibrationEffect.MAX_AMPLITUDE).
        return when (effect) {
            HapticEffect.PRESS -> intArrayOf(0, (255f * scale).toInt())
            HapticEffect.RELEASE -> intArrayOf(0, (180f * scale).toInt())
            HapticEffect.JOYSTICK -> intArrayOf(0, (128f * scale).toInt())
        }
    }

    /**
     * Get the haptic feedback constant that matches the [effect].
     *
     * @param effect The [HapticEffect] of the feedback.
     * @return The matching [HapticFeedbackConstants] constant.
     */
    private fun getHapticFeedbackConstant(effect: HapticEffect): Int {
        return when (effect) {
            HapticEffect.PRESS -> HapticFeedbackConstants.VIRTUAL_KEY
            HapticEffect.RELEASE -> releaseHapticFeedbackConstant
            HapticEffect.JOYSTICK -> HapticFeedbackConstants.CLOCK_TICK
        }
    }

    @RequiresApi(Build.VERSION_CODES.S)
    private fun getPrimitive(effect: HapticEffect): Int {
        return when (effect) {
            HapticEffect.PRESS -> VibrationEffect.Composition.PRIMITIVE_QUICK_FALL
            HapticEffect.RELEASE -> VibrationEffect.Composition.PRIMITIVE_QUICK_RISE
            HapticEffect.JOYSTICK -> VibrationEffect.Composition.PRIMITIVE_LOW_TICK
        }
    }

    private fun areAllPrimitivesSupported(): Boolean {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.S && vibrator.areAllPrimitivesSupported(
            *HapticEffect.entries.map { getPrimitive(it) }.toIntArray()
        )
    }
}

enum class HapticEffect {
    PRESS,
    RELEASE,
    JOYSTICK
}
