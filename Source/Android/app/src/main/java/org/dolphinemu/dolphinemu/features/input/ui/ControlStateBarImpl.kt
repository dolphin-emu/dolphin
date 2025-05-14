package org.dolphinemu.dolphinemu.features.input.ui

import android.view.View
import android.widget.LinearLayout
import androidx.core.math.MathUtils

class ControlStateBarImpl(private val filledView: View, private val unfilledView: View) {
    private var state_ = 0.0f

    var state: Float
        get() = state_
        set(value) {
            val clampedState = MathUtils.clamp(value, 0.0f, 1.0f)
            if (clampedState != state_) {
                setLinearLayoutWeight(filledView, clampedState)
                setLinearLayoutWeight(unfilledView, 1.0f - clampedState)
            }
            state_ = clampedState
        }

    companion object {
        private fun setLinearLayoutWeight(view: View, weight: Float) {
            val layoutParams = view.layoutParams as LinearLayout.LayoutParams
            layoutParams.weight = weight
            view.layoutParams = layoutParams
        }
    }
}
