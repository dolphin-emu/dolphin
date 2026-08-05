// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.lifecycle.ViewModelProvider
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.features.settings.model.PostProcessing
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting


class ShaderOptionsFragment : Fragment() {

    private lateinit var shaderName: String
    private lateinit var recyclerView: RecyclerView
    private var adapter: ShaderOptionsAdapter? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        arguments?.let {
            shaderName = it.getString(ARG_SHADER_NAME, "")
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_shader_options, container, false)
        recyclerView = view.findViewById(R.id.shader_options_list)
        recyclerView.layoutManager = LinearLayoutManager(context)
        return view
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

// Makes sure to save and load the selected shader so its restored correctly on next boot.
        val settings = ViewModelProvider(requireActivity())[SettingsViewModel::class.java].settings
        StringSetting.GFX_ENHANCE_POST_SHADER.setString(settings, shaderName)
        settings.saveSettings()

        val options = PostProcessing.getShaderOptions(shaderName)
        if (options != null) {
            adapter = ShaderOptionsAdapter(shaderName, options.options)
            recyclerView.adapter = adapter
            // Save all settings after change instantly to prevent stale values.
            adapter?.saveAll()
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        adapter = null
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
