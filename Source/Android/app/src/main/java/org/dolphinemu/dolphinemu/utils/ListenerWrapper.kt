package org.dolphinemu.dolphinemu.utils

import android.widget.CompoundButton
import androidx.core.view.HapticFeedbackConstantsCompat
import com.google.android.material.slider.Slider

/**
 * Wrapper that enhances listeners with additional features, such as haptic feedback.
 */
object ListenerWrapper {
    private val hapticsProvider = HapticsProvider(null)

    fun wrapOnChangeListener(listener: Slider.OnChangeListener?): Slider.OnChangeListener {
        return Slider.OnChangeListener { slider: Slider, value: Float, fromUser: Boolean ->
            listener?.onValueChange(slider, value, fromUser)
            if (fromUser) {
                hapticsProvider.performHapticFeedback(
                    slider, HapticFeedbackConstantsCompat.CLOCK_TICK, false
                )
            }
        }
    }

    fun wrapOnCheckedChangeListener(listener: CompoundButton.OnCheckedChangeListener?): CompoundButton.OnCheckedChangeListener {
        return CompoundButton.OnCheckedChangeListener { buttonView: CompoundButton?, isChecked: Boolean ->
            listener?.onCheckedChanged(buttonView, isChecked)
            if (buttonView != null) {
                /** Using old constants because [HapticFeedbackConstantsCompat.TOGGLE_ON]
                 *  and [HapticFeedbackConstantsCompat.TOGGLE_OFF] don't seem to work. */
                val feedbackConstant =
                    HapticFeedbackConstantsCompat.CONTEXT_CLICK.takeIf { isChecked }
                        ?: HapticFeedbackConstantsCompat.CLOCK_TICK
                hapticsProvider.performHapticFeedback(buttonView, feedbackConstant, false)
            }
        }
    }
}
