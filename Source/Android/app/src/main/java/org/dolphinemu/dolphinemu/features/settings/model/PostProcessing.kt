// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model

object PostProcessing {
    @JvmStatic
    val shaderList: Array<String>
        external get

    @JvmStatic
    val anaglyphShaderList: Array<String>
        external get

    @JvmStatic
    val passiveShaderList: Array<String>
        external get

    data class ShaderOptions(val options: List<ShaderOption>)

    sealed class ShaderOption(
        val type: Int,
        val guiName: String,
        val optionName: String,
        val dependentOption: String
    ) {
        class Bool(
            guiName: String,
            optionName: String,
            dependentOption: String,
            var value: Boolean
        ) : ShaderOption(0, guiName, optionName, dependentOption)

        class Float(
            guiName: String,
            optionName: String,
            dependentOption: String,
            var values: FloatArray,
            val minValues: FloatArray,
            val maxValues: FloatArray,
            val stepValues: FloatArray
        ) : ShaderOption(1, guiName, optionName, dependentOption)

        class Integer(
            guiName: String,
            optionName: String,
            dependentOption: String,
            var values: IntArray,
            val minValues: IntArray,
            val maxValues: IntArray,
            val stepValues: IntArray
        ) : ShaderOption(2, guiName, optionName, dependentOption)
    }

    @JvmStatic
    external fun getShaderOptions(shaderName: String): ShaderOptions?

    @JvmStatic
    external fun setShaderOption(shaderName: String, optionName: String, value: Boolean)

    @JvmStatic
    external fun setShaderOption(shaderName: String, optionName: String, values: FloatArray)

    @JvmStatic
    external fun setShaderOption(shaderName: String, optionName: String, values: IntArray)
}
