package org.dolphinemu.dolphinemu.utils

import android.view.View
import android.widget.CompoundButton
import androidx.core.view.HapticFeedbackConstantsCompat
import androidx.core.view.ViewCompat
import com.google.android.material.slider.Slider

/**
 * Wrapper object that enhances listeners with haptic feedback.
 */
object HapticListener {

    /**
     * Wraps a [View.OnClickListener] with haptic feedback.
     *
     * @param listener The [View.OnClickListener] to be wrapped. Can be null.
     * @param feedbackConstant The haptic feedback constant to be used for haptic feedback.
     * Defaults to [HapticFeedbackConstantsCompat.CONTEXT_CLICK] if not specified.
     * @return A new listener which wraps [listener] with haptic feedback.
     */
    fun wrapOnClickListener(
        listener: View.OnClickListener?,
        feedbackConstant: Int = HapticFeedbackConstantsCompat.CONTEXT_CLICK
    ): View.OnClickListener {
        return View.OnClickListener { view: View ->
            listener?.onClick(view)
            ViewCompat.performHapticFeedback(view, feedbackConstant)
        }
    }

    /**
     * Wraps a [Slider.OnChangeListener] with haptic feedback.
     * Feedback is provided at intervals of 5% to prevent excessive vibrations.
     *
     * @param listener The [Slider.OnChangeListener] to be wrapped. Can be null.
     * @param initialValue The value used to initialize the slider.
     * @return A new listener which wraps [listener] with haptic feedback.
     */
    fun wrapOnChangeListener(listener: Slider.OnChangeListener?, initialValue: Float): Slider.OnChangeListener {
        var previousValue = initialValue
        return Slider.OnChangeListener { slider: Slider, value: Float, fromUser: Boolean ->
            listener?.onValueChange(slider, value, fromUser)
            if (fromUser) {
                val interval = (slider.valueTo - slider.valueFrom) * 0.05f
                val previousInterval = kotlin.math.round(previousValue / interval) * interval
                val valueChange = kotlin.math.abs(value - previousInterval)
                val feedbackConstant = if (value == slider.valueFrom || value == slider.valueTo) {
                    HapticFeedbackConstantsCompat.CONTEXT_CLICK
                } else if (value == previousValue || valueChange >= interval) {
                    HapticFeedbackConstantsCompat.CLOCK_TICK
                } else {
                    HapticFeedbackConstantsCompat.NO_HAPTICS
                }
                if (feedbackConstant != HapticFeedbackConstantsCompat.NO_HAPTICS) {
                    ViewCompat.performHapticFeedback(slider, feedbackConstant)
                    previousValue = value
                }
            } else {
                previousValue = value
            }
        }
    }

    /**
     * Wraps a [CompoundButton.OnCheckedChangeListener] with haptic feedback.
     *
     * @param listener The [CompoundButton.OnCheckedChangeListener] to be wrapped. Can be null.
     * @return A new listener which wraps [listener] with haptic feedback.
     */
    fun wrapOnCheckedChangeListener(listener: CompoundButton.OnCheckedChangeListener?): CompoundButton.OnCheckedChangeListener {
        return CompoundButton.OnCheckedChangeListener { buttonView: CompoundButton, isChecked: Boolean ->
            listener?.onCheckedChanged(buttonView, isChecked)
            /** Using old constants because [HapticFeedbackConstantsCompat.TOGGLE_ON]
             *  and [HapticFeedbackConstantsCompat.TOGGLE_OFF] don't seem to work. */
            val feedbackConstant = if (buttonView.isChecked) {
                HapticFeedbackConstantsCompat.CONTEXT_CLICK
            } else HapticFeedbackConstantsCompat.CLOCK_TICK
            ViewCompat.performHapticFeedback(buttonView, feedbackConstant)
        }
    }
}
