package org.dolphinemu.dolphinemu.features.input.ui

import android.content.Context
import android.util.AttributeSet
import android.view.LayoutInflater
import android.widget.LinearLayout
import org.dolphinemu.dolphinemu.databinding.ViewControlStateBarHorizontalBinding

class ControlStateBarHorizontal @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : LinearLayout(context, attrs, defStyleAttr) {

    private val binding =
        ViewControlStateBarHorizontalBinding.inflate(LayoutInflater.from(context), this)

    private val impl = ControlStateBarImpl(binding.viewFilled, binding.viewUnfilled)

    var state: Float
        get() = impl.state
        set(value) {
            impl.state = value
        }
}
