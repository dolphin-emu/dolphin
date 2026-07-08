package org.dolphinemu.dolphinemu.model

import android.os.Build
import android.os.VibrationEffect
import android.os.Vibrator
import androidx.annotation.FloatRange
import androidx.annotation.RequiresApi
import androidx.core.view.HapticFeedbackConstantsCompat

/**
 * Enum to represent haptic effects.
 *
 * @param feedbackConstant One of the constants defined in [HapticFeedbackConstantsCompat].
 * @param maxTimings The maximum timing values, in milliseconds, of the timing / amplitude pairs.
 * Note that lower-end vibrators may not be able to vibrate for short (<50ms) durations.
 * @param maxAmplitudes The maximum amplitude values of the timing / amplitude pairs.
 * Amplitude values must be between 0 and 255. An amplitude value of 0 implies the motor is off.
 * @param primaryPrimitive The primary primitive ID, or null if the SDK version is too low.
 * @param fallbackPrimitive The fallback primitive ID, or null if the SDK version is too low.
 */
enum class HapticEffect(
    val feedbackConstant: Int,
    private val maxTimings: LongArray,
    private val maxAmplitudes: IntArray,
    val primaryPrimitive: Int?,
    val fallbackPrimitive: Int? = null
) {
    PRESS(
        feedbackConstant = HapticFeedbackConstantsCompat.VIRTUAL_KEY,
        maxTimings = longArrayOf(0L, 100L),
        maxAmplitudes = intArrayOf(0, 180),
        primaryPrimitive = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            VibrationEffect.Composition.PRIMITIVE_QUICK_FALL
        } else {
            null
        },
        fallbackPrimitive = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            VibrationEffect.Composition.PRIMITIVE_CLICK
        } else {
            null
        }
    ),
    RELEASE(
        feedbackConstant = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1) {
            HapticFeedbackConstantsCompat.VIRTUAL_KEY_RELEASE
        } else {
            HapticFeedbackConstantsCompat.CONTEXT_CLICK // Better than a no-op.
        },
        maxTimings = longArrayOf(0L, 70L),
        maxAmplitudes = intArrayOf(0, 128),
        primaryPrimitive = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            VibrationEffect.Composition.PRIMITIVE_QUICK_RISE
        } else {
            null
        }
    ),
    JOYSTICK(
        feedbackConstant = HapticFeedbackConstantsCompat.SEGMENT_FREQUENT_TICK,
        maxTimings = longArrayOf(50L),
        maxAmplitudes = intArrayOf(90),
        primaryPrimitive = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            VibrationEffect.Composition.PRIMITIVE_LOW_TICK
        } else {
            null
        },
        fallbackPrimitive = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            VibrationEffect.Composition.PRIMITIVE_TICK
        } else {
            null
        }
    );

    fun getMaxTimings(): LongArray = maxTimings.copyOf()

    @RequiresApi(Build.VERSION_CODES.O)
    fun getMaxAmplitudes(): IntArray = maxAmplitudes.copyOf()

    fun scaleTimings(@FloatRange(from = 0.0, to = 1.0) scale: Float): LongArray =
        maxTimings.map { (it.toFloat() * scale).toLong() }.toLongArray()

    @RequiresApi(Build.VERSION_CODES.O)
    fun scaleAmplitudes(@FloatRange(from = 0.0, to = 1.0) scale: Float): IntArray =
        maxAmplitudes.map { (it.toFloat() * scale).toInt() }.toIntArray()

    /**
     * Gets this effect's preferred primitive ID that is supported by the provided [vibrator].
     * @param vibrator A [Vibrator] instance for checking whether primitives are supported by it.
     * @return The best supported primitive ID of this effect, or null if there is none.
     */
    @RequiresApi(Build.VERSION_CODES.R)
    fun getSupportedPrimitive(vibrator: Vibrator): Int? =
        listOf(primaryPrimitive, fallbackPrimitive).firstOrNull {
            it != null && vibrator.areAllPrimitivesSupported(it)
        }
}
