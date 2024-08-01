// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.os.Build
import android.os.VibrationEffect
import android.os.Vibrator
import android.view.View
import androidx.annotation.RequiresApi
import androidx.core.view.HapticFeedbackConstantsCompat
import androidx.core.view.ViewCompat
import org.dolphinemu.dolphinemu.features.input.model.DolphinVibratorManagerFactory
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting

/**
 * This class provides methods that facilitate performing haptic feedback.
 *
 * @property vibrator The [Vibrator] instance to be used for (fallback) vibration.
 * Can be null. Defaults to the system default vibrator.
 */
class HapticsProvider(
    private val vibrator: Vibrator? =
        DolphinVibratorManagerFactory.getSystemVibratorManager().getDefaultVibrator()
) {

    /**
     * Perform a [feedbackConstant] type of haptic feedback.
     * If a [fallback] vibration is desired, [vibrate] is called for a vibration
     * with an intensity scaled to mimic the [feedbackConstant].
     *
     * @param view The [View] to perform haptic feedback on.
     * @param feedbackConstant A haptic feedback constant from [HapticFeedbackConstantsCompat].
     * @param fallback Whether to fallback to a scaled vibration. Defaults to the relevant setting.
     */
    fun performHapticFeedback(
        view: View,
        feedbackConstant: Int,
        fallback: Boolean = BooleanSetting.MAIN_FALLBACK_HAPTICS.boolean
    ) {
        if (fallback) {
            val scale = getVibrationScale(feedbackConstant)
            val scaledDuration =
                (IntSetting.MAIN_HAPTICS_DURATION.int * scale).toLong().coerceAtLeast(1L)
            val scaledAmplitude =
                (IntSetting.MAIN_HAPTICS_AMPLITUDE.int * scale).toInt().coerceIn(1, 255)
            vibrate(scaledDuration, scaledAmplitude)
        } else {
            ViewCompat.performHapticFeedback(
                view,
                feedbackConstant,
                HapticFeedbackConstantsCompat.FLAG_IGNORE_VIEW_SETTING
            )
        }
    }

    /**
     * Vibrate for the specified [duration] with [amplitude] strength (if [vibrator] is capable).
     *
     * @param duration The number of milliseconds to vibrate. This must be a positive number.
     * @param amplitude The strength of the vibration. This must be a value between 1 and 255,
     * or [VibrationEffect.DEFAULT_AMPLITUDE].
     */
    fun vibrate(duration: Long = DEFAULT_DURATION, amplitude: Int = DEFAULT_AMPLITUDE) {
        if (!hasVibrator()) return
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val timings = longArrayOf(0, duration)
            val amplitudes = intArrayOf(0, amplitude.takeIf { hasAmplitudeControl() }
                ?: VibrationEffect.DEFAULT_AMPLITUDE)
            vibrator!!.vibrate(VibrationEffect.createWaveform(timings, amplitudes, -1))
        } else {
            vibrator!!.vibrate(duration)
        }
    }

    /**
     * Determine a scaling factor for the duration and amplitude of a vibration
     * based on the type of the provided [feedbackConstant].
     *
     * For creating a weak and strong vibration effect, it is recommended that the scales
     * differ by a ratio of 1.4 or more, so the difference in intensity can be easily perceived.
     *
     * @param feedbackConstant A haptic feedback constant from [HapticFeedbackConstantsCompat].
     * @return A scaling factor for the desired vibration.
     */
    private fun getVibrationScale(feedbackConstant: Int): Float {
        return when (feedbackConstant) {
            HapticFeedbackConstantsCompat.VIRTUAL_KEY_RELEASE -> 0.7f
            HapticFeedbackConstantsCompat.CLOCK_TICK -> 0.5f
            else -> 1.0f
        }
    }

    // Passthrough methods used to determine the vibrator capabilities.
    fun hasVibrator(): Boolean = vibrator?.hasVibrator() ?: false

    @RequiresApi(Build.VERSION_CODES.O)
    fun hasAmplitudeControl(): Boolean = vibrator?.hasAmplitudeControl() ?: false

    companion object {
        // Default vibration values for a non-buzzy click sensation.
        const val DEFAULT_DURATION = 20L
        const val DEFAULT_AMPLITUDE = 255
    }
}
