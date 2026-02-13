package org.dolphinemu.dolphinemu.features.settings.ui

import android.os.Bundle
import android.view.View
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.features.settings.model.PostProcessing

class ShaderOptionsFragment : BaseSettingsFragment() {

    private lateinit var shaderName: String
    private lateinit var adapter: ShaderOptionsAdapter

    override fun getLayoutId() = R.layout.fragment_shader_options

    override fun getRecyclerViewId() = R.id.shader_options_list

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        arguments?.let {
            shaderName = it.getString(ARG_SHADER_NAME, "")
        }
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val options = PostProcessing.getShaderOptions(shaderName)
        if (options != null) {
            adapter = ShaderOptionsAdapter(shaderName, options.options)
            recyclerView.adapter = adapter
        }
    }

    companion object {
        private const val ARG_SHADER_NAME = "shader_name"

        fun newInstance(shaderName: String): ShaderOptionsFragment {
            val fragment = ShaderOptionsFragment()
            val args = Bundle()
            args.putString(ARG_SHADER_NAME, shaderName)
            fragment.arguments = args
            return fragment
        }
    }
}
