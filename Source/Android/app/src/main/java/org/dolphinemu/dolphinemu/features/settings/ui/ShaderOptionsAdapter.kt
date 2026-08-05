// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.slider.Slider
import com.google.android.material.switchmaterial.SwitchMaterial
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.features.settings.model.PostProcessing
import java.math.BigDecimal
import java.math.RoundingMode

class ShaderOptionsAdapter(
    private val shaderName: String,
    rawOptions: List<PostProcessing.ShaderOption>
) : RecyclerView.Adapter<RecyclerView.ViewHolder>() {

    private val options: List<PostProcessing.ShaderOption>

    companion object {
        private const val TYPE_BOOL = 0
        private const val TYPE_FLOAT_SINGLE = 1
        private const val TYPE_INT_SINGLE = 2
        private const val TYPE_FLOAT_MULTI = 3
        private const val TYPE_INT_MULTI = 4
    }

    // Maps option name > shaderoptions for fast lookup
    private val optionsByName: Map<String, PostProcessing.ShaderOption>

    // reorder options so bools appear before children lists
    private fun reorderOptions(
        options: List<PostProcessing.ShaderOption>
    ): List<PostProcessing.ShaderOption> {
        if (options.isEmpty()) return emptyList()

        // prevent duplication while maintianing insertion
        val seen = LinkedHashSet<PostProcessing.ShaderOption>()

        // recursive adds an option and all its dependents to the seen set
        fun addWithDependents(option: PostProcessing.ShaderOption) {
            if (!seen.add(option)) return
            options.filter { it.dependentOption == option.optionName }
                .forEach { addWithDependents(it) }
        }
// checks non parent dependencies first.
        val roots = options.filter { it.dependentOption.isEmpty() }
        roots.forEach { addWithDependents(it) }
// catch orphaned options
        options.filter { it !in seen }.forEach { seen.add(it) }
        return seen.toList()
    }

    init {

        options = reorderOptions(rawOptions)
        optionsByName = options.associateBy { it.optionName }
        //clamp and round numeric values to valid step allignments. Prevents out of range values
        options.forEach { option ->
            when (option) {
                is PostProcessing.ShaderOption.Float -> {
                    for (i in option.values.indices) {
                        val clamped = option.values[i].coerceIn(
                            option.minValues[i], option.maxValues[i]
                        )
                        option.values[i] = roundToStep(
                            clamped, option.minValues[i], option.stepValues[i]
                        )
                    }
                }

                is PostProcessing.ShaderOption.Integer -> {
                    for (i in option.values.indices) {
                        val clamped = option.values[i].coerceIn(
                            option.minValues[i], option.maxValues[i]
                        )
                        option.values[i] = roundToStep(
                            clamped.toFloat(),
                            option.minValues[i].toFloat(),
                            option.stepValues[i].toFloat()
                        ).toInt()
                    }
                }

                is PostProcessing.ShaderOption.Bool -> {}
            }
        }
    }

    // recursively checks if an option is enabled by checking the chain
    // Disables/greys out options if they are not enabled in the chain
    private fun isEnabled(option: PostProcessing.ShaderOption): Boolean {
        if (option.dependentOption.isEmpty()) return true
        val parent = optionsByName[option.dependentOption] ?: return true
        if (parent is PostProcessing.ShaderOption.Bool && !parent.value) return false
        return isEnabled(parent)
    }

    // checks dependent options to order list.
    private fun isDependent(option: PostProcessing.ShaderOption, parentName: String): Boolean {
        if (option.dependentOption.isEmpty()) return false
        if (option.dependentOption == parentName) return true
        val parent = optionsByName[option.dependentOption] ?: return false
        return isDependent(parent, parentName)
    }

    // Force save settings when slider options are changed
    fun saveAll() {
        options.forEach { option ->
            when (option) {
                is PostProcessing.ShaderOption.Bool ->
                    PostProcessing.setShaderOption(shaderName, option.optionName, option.value)

                is PostProcessing.ShaderOption.Float ->
                    PostProcessing.setShaderOption(shaderName, option.optionName, option.values)

                is PostProcessing.ShaderOption.Integer ->
                    PostProcessing.setShaderOption(shaderName, option.optionName, option.values)
            }
        }
    }

    // Tries to return how many decimal places the step size uses to work around setting stepsize too low
    // Step size of 0.1 -> 1 minimum value should be 0.1 and not 0 which causes a lockout of the settings
    private fun decimalPlaces(step: Float): Int {
        if (step == 0f) return 2
        return BigDecimal(step.toString()).stripTrailingZeros().scale()
    }

    // Uses BigDecimal to avoid point drift. Step size of 0.1 shouldn't produce 0.09999 etc
    private fun roundToStep(value: Float, valueFrom: Float, stepSize: Float): Float {
        if (stepSize == 0f) return value
        val dp = decimalPlaces(stepSize)
        val bd = BigDecimal(value.toString())
        val from = BigDecimal(valueFrom.toString())
        val step = BigDecimal(stepSize.toString())
        val steps = (bd - from).divide(step, 0, RoundingMode.HALF_UP).toInt()
        return (from + step.multiply(BigDecimal(steps)))
            .setScale(dp, RoundingMode.HALF_UP)
            .toFloat()
    }

    // Formats float to same decimal places as stepsize.
    private fun formatValue(value: Float, stepSize: Float): String {
        if (stepSize == 0f) return value.toString()
        val dp = decimalPlaces(stepSize)
        return BigDecimal(value.toString())
            .setScale(dp, RoundingMode.HALF_UP)
            .stripTrailingZeros()
            .toPlainString()
    }

    private fun onRelease(action: () -> Unit) = object : Slider.OnSliderTouchListener {
        override fun onStartTrackingTouch(slider: Slider) {}
        override fun onStopTrackingTouch(slider: Slider) = action()
    }

    private fun bindFloatSlider(
        slider: Slider,
        valueView: TextView,
        option: PostProcessing.ShaderOption.Float,
        index: Int,
        enabled: Boolean
    ) {
        val current = option.values[index]
        // Reset stepsize to 0f before setting ValueFrom/valueTo.
        // Avoids crashes if previous step doesn't divide evenly to a new range.
        slider.clearOnChangeListeners()
        slider.clearOnSliderTouchListeners()
        slider.isEnabled = enabled
        slider.stepSize = 0f
        slider.valueFrom = option.minValues[index]
        slider.valueTo = option.maxValues[index]
        slider.value = current
        slider.stepSize = option.stepValues[index]
        valueView.text = formatValue(current, option.stepValues[index])
        slider.addOnChangeListener { _, v, _ ->
            val rounded = roundToStep(v, option.minValues[index], option.stepValues[index])
            option.values[index] = rounded
            valueView.text = formatValue(rounded, option.stepValues[index])
        }
        slider.addOnSliderTouchListener(onRelease {
            PostProcessing.setShaderOption(shaderName, option.optionName, option.values)
        })
    }

    private fun bindIntSlider(
        slider: Slider,
        valueView: TextView,
        option: PostProcessing.ShaderOption.Integer,
        index: Int,
        enabled: Boolean
    ) {
        val current = option.values[index]
        slider.clearOnChangeListeners()
        slider.clearOnSliderTouchListeners()
        slider.isEnabled = enabled
        slider.stepSize = 0f
        slider.valueFrom = option.minValues[index].toFloat()
        slider.valueTo = option.maxValues[index].toFloat()
        slider.value = current.toFloat()
        slider.stepSize = option.stepValues[index].toFloat()
        valueView.text = current.toString()
        slider.addOnChangeListener { _, v, _ ->
            val rounded = roundToStep(
                v,
                option.minValues[index].toFloat(),
                option.stepValues[index].toFloat()
            ).toInt()
            option.values[index] = rounded
            valueView.text = rounded.toString()
        }
        slider.addOnSliderTouchListener(onRelease {
            PostProcessing.setShaderOption(shaderName, option.optionName, option.values)
        })
    }

    override fun getItemViewType(position: Int): Int {
        return when (val opt = options[position]) {
            is PostProcessing.ShaderOption.Bool -> TYPE_BOOL
            is PostProcessing.ShaderOption.Float ->
                if (opt.values.size > 1) TYPE_FLOAT_MULTI
                else TYPE_FLOAT_SINGLE

            is PostProcessing.ShaderOption.Integer ->
                if (opt.values.size > 1) TYPE_INT_MULTI
                else TYPE_INT_SINGLE
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int):
        RecyclerView.ViewHolder {
        val inf = LayoutInflater.from(parent.context)
        return when (viewType) {
            TYPE_BOOL -> BoolViewHolder(
                inf.inflate(
                    R.layout.list_item_shader_option_boolean, parent, false
                )
            )

            TYPE_FLOAT_SINGLE -> FloatSingleViewHolder(
                inf.inflate(
                    R.layout.list_item_shader_option_float, parent, false
                )
            )

            TYPE_INT_SINGLE -> IntSingleViewHolder(
                inf.inflate(
                    R.layout.list_item_shader_option_integer, parent, false
                )
            )

            TYPE_FLOAT_MULTI -> FloatMultiViewHolder(
                inf.inflate(
                    R.layout.list_item_shader_option_multi, parent, false
                )
            )

            TYPE_INT_MULTI -> IntMultiViewHolder(
                inf.inflate(R.layout.list_item_shader_option_multi, parent, false)
            )

            else -> throw IllegalArgumentException("Unknown view type $viewType")
        }
    }


    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        val option = options[position]
        val enabled = isEnabled(option)
        when (holder) {
            is BoolViewHolder -> holder.bind(
                option as PostProcessing.ShaderOption.Bool, enabled
            )

            is FloatSingleViewHolder -> holder.bind(
                option as PostProcessing.ShaderOption.Float, enabled
            )

            is IntSingleViewHolder -> holder.bind(
                option as PostProcessing.ShaderOption.Integer, enabled
            )

            is FloatMultiViewHolder -> holder.bind(
                option as PostProcessing.ShaderOption.Float, enabled
            )

            is IntMultiViewHolder -> holder.bind(
                option as PostProcessing.ShaderOption.Integer, enabled
            )
        }
    }

    override fun getItemCount(): Int = options.size

    inner class BoolViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        private val switch: SwitchMaterial = itemView.findViewById(R.id.switch_option)
        fun bind(option: PostProcessing.ShaderOption.Bool, enabled: Boolean) {
            switch.text = option.guiName
            switch.isEnabled = enabled
            switch.setOnCheckedChangeListener(null)
            switch.isChecked = option.value
            switch.setOnCheckedChangeListener { _, isChecked ->
                option.value = isChecked
                PostProcessing.setShaderOption(shaderName, option.optionName, isChecked)
                itemView.post {
                    options.forEachIndexed { index, opt ->
                        if (isDependent(opt, option.optionName)) {
                            notifyItemChanged(index)
                        }
                    }
                }
            }
        }
    }

    inner class FloatSingleViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        private val name: TextView = itemView.findViewById(R.id.text_option_name)
        private val slider: Slider = itemView.findViewById(R.id.slider_option)
        private val value: TextView = itemView.findViewById(R.id.text_option_value)
        fun bind(option: PostProcessing.ShaderOption.Float, enabled: Boolean) {
            name.text = option.guiName
            name.isEnabled = enabled
            bindFloatSlider(slider, value, option, 0, enabled)
        }
    }

    inner class IntSingleViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        private val name: TextView = itemView.findViewById(R.id.text_option_name)
        private val slider: Slider = itemView.findViewById(R.id.slider_option)
        private val value: TextView = itemView.findViewById(R.id.text_option_value)
        fun bind(option: PostProcessing.ShaderOption.Integer, enabled: Boolean) {
            name.text = option.guiName
            name.isEnabled = enabled
            bindIntSlider(slider, value, option, 0, enabled)
        }

    }

    inner class FloatMultiViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        private val name: TextView = itemView.findViewById(R.id.text_option_name)
        private val container: LinearLayout = itemView.findViewById(R.id.container)
        fun bind(option: PostProcessing.ShaderOption.Float, enabled: Boolean) {
            name.text = option.guiName
            name.isEnabled = enabled
            container.removeAllViews()
            val inflater = LayoutInflater.from(itemView.context)
            for (i in option.values.indices) {
                val row = inflater.inflate(R.layout.list_item_shader_option_float, container, false)
                row.findViewById<TextView>(R.id.text_option_name).visibility = View.GONE
                val slider = row.findViewById<Slider>(R.id.slider_option)
                val value = row.findViewById<TextView>(R.id.text_option_value)
                bindFloatSlider(slider, value, option, i, enabled)
                container.addView(row)
            }
        }
    }

    inner class IntMultiViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        private val name: TextView = itemView.findViewById(R.id.text_option_name)
        private val container: LinearLayout = itemView.findViewById(R.id.container)
        fun bind(option: PostProcessing.ShaderOption.Integer, enabled: Boolean) {
            name.text = option.guiName
            name.isEnabled = enabled
            container.removeAllViews()
            val inflater = LayoutInflater.from(itemView.context)
            for (i in option.values.indices) {
                val row =
                    inflater.inflate(R.layout.list_item_shader_option_integer, container, false)
                row.findViewById<TextView>(R.id.text_option_name).visibility = View.GONE
                val slider = row.findViewById<Slider>(R.id.slider_option)
                val value = row.findViewById<TextView>(R.id.text_option_value)
                bindIntSlider(slider, value, option, i, enabled)
                container.addView(row)
            }
        }
    }
}
