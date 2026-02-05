package org.dolphinemu.dolphinemu.features.settings.ui

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.slider.Slider
import com.google.android.material.switchmaterial.SwitchMaterial
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.features.settings.model.PostProcessing

class ShaderOptionsAdapter(private val shaderName: String, private val options: List<PostProcessing.ShaderOption>) :
    RecyclerView.Adapter<RecyclerView.ViewHolder>() {

    override fun getItemViewType(position: Int): Int {
        return options[position].type
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        return when (viewType) {
            0 -> {
                val view = inflater.inflate(R.layout.list_item_shader_option_boolean, parent, false)
                BoolViewHolder(view)
            }
            1 -> {
                val view = inflater.inflate(R.layout.list_item_shader_option_float, parent, false)
                FloatViewHolder(view)
            }
            else -> {
                val view = inflater.inflate(R.layout.list_item_shader_option_integer, parent, false)
                IntViewHolder(view)
            }
        }
    }

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        when (holder) {
            is BoolViewHolder -> holder.bind(options[position] as PostProcessing.ShaderOption.Bool)
            is FloatViewHolder -> holder.bind(options[position] as PostProcessing.ShaderOption.Float)
            is IntViewHolder -> holder.bind(options[position] as PostProcessing.ShaderOption.Integer)
        }
    }

    override fun getItemCount(): Int {
        return options.size
    }

    inner class BoolViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        private val switch: SwitchMaterial = itemView.findViewById(R.id.switch_option)

        fun bind(option: PostProcessing.ShaderOption.Bool) {
            switch.text = option.guiName
            switch.isChecked = option.value
            switch.setOnCheckedChangeListener { _, isChecked ->
                option.value = isChecked
                PostProcessing.setShaderOption(shaderName, option.optionName, isChecked)
            }
        }
    }

    inner class FloatViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        private val name: TextView = itemView.findViewById(R.id.text_option_name)
        private val slider: Slider = itemView.findViewById(R.id.slider_option)
        private val value: TextView = itemView.findViewById(R.id.text_option_value)

        fun bind(option: PostProcessing.ShaderOption.Float) {
            name.text = option.guiName
            if (option.values.isNotEmpty()) {
                slider.valueFrom = option.minValues[0]
                slider.valueTo = option.maxValues[0]
                slider.stepSize = option.stepValues[0]
                slider.value = option.values[0]
                value.text = option.values[0].toString()

                slider.addOnChangeListener { _, sliderValue, _ ->
                    option.values[0] = sliderValue
                    value.text = sliderValue.toString()
                    PostProcessing.setShaderOption(shaderName, option.optionName, option.values)
                }
            }
        }
    }

    inner class IntViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        private val name: TextView = itemView.findViewById(R.id.text_option_name)
        private val slider: Slider = itemView.findViewById(R.id.slider_option)
        private val value: TextView = itemView.findViewById(R.id.text_option_value)

        fun bind(option: PostProcessing.ShaderOption.Integer) {
            name.text = option.guiName
            if (option.values.isNotEmpty()) {
                slider.valueFrom = option.minValues[0].toFloat()
                slider.valueTo = option.maxValues[0].toFloat()
                slider.stepSize = option.stepValues[0].toFloat()
                slider.value = option.values[0].toFloat()
                value.text = option.values[0].toString()

                slider.addOnChangeListener { _, sliderValue, _ ->
                    option.values[0] = sliderValue.toInt()
                    value.text = sliderValue.toInt().toString()
                    PostProcessing.setShaderOption(shaderName, option.optionName, option.values)
                }
            }
        }
    }
}
