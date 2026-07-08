// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.os.Build
import android.os.VibrationEffect
import android.os.Vibrator
import android.view.View
import androidx.annotation.FloatRange
import androidx.core.view.ViewCompat
import org.dolphinemu.dolphinemu.features.input.model.DolphinVibratorManagerFactory
import org.dolphinemu.dolphinemu.model.HapticEffect

/**
 * Provides haptic feedback to the user.
 *
 * @property vibrator The [Vibrator] instance to be used for vibration.
 * Defaults to the system's default vibrator.
 */
class HapticsProvider(
    private val vibrator: Vibrator =
        DolphinVibratorManagerFactory.getSystemVibratorManager().getDefaultVibrator()
) {
    private val supportedPrimitivesMap: Map<HapticEffect, Int?>
    private val primitiveSupport: Boolean

    init {
        supportedPrimitivesMap = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            HapticEffect.entries.associateWith { it.getSupportedPrimitive(vibrator) }
        } else {
            HapticEffect.entries.associateWith { null }
        }
        primitiveSupport = supportedPrimitivesMap.values.all { it != null }
    }

    /**
     * Perform haptic feedback natively (if a [View] is provided),
     * or by composing primitives (if supported), falling back to a waveform or a legacy vibration.
     *
     * @param effect The [HapticEffect] of the feedback.
     * @param view The [View] to perform the feedback on, can be null to use the [vibrator] directly.
     * @param scale The intensity scale of the feedback, will only be used if [view] is not set.
     */
    fun provideFeedback(
        effect: HapticEffect,
        view: View? = null,
        @FloatRange(from = 0.0, to = 1.0) scale: Float = 0.5f
    ) {
        if (view != null) {
            ViewCompat.performHapticFeedback(view, effect.feedbackConstant)
        } else if (primitiveSupport && Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            vibrator.vibrate(
                VibrationEffect
                    .startComposition()
                    .addPrimitive(supportedPrimitivesMap[effect]!!, scale)
                    .compose()
            )
        } else {
            val scaledTimings = effect.scaleTimings(scale)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                vibrator.vibrate(
                    VibrationEffect.createWaveform(scaledTimings, effect.scaleAmplitudes(scale), -1)
                )
            } else {
                vibrator.vibrate(scaledTimings.sum())
            }
        }
    }
}
