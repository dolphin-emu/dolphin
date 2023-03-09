// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.ColorInt
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModelProvider
import androidx.recyclerview.widget.LinearLayoutManager
import com.google.android.material.color.MaterialColors
import com.google.android.material.elevation.ElevationOverlayProvider
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.databinding.FragmentCheatListBinding
import org.dolphinemu.dolphinemu.features.cheats.model.CheatsViewModel
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsDividerItemDecoration

class CheatListFragment : Fragment() {
    private var _binding: FragmentCheatListBinding? = null
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentCheatListBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        val activity = requireActivity() as CheatsActivity
        val viewModel = ViewModelProvider(activity)[CheatsViewModel::class.java]

        binding.cheatList.adapter = CheatsAdapter(activity, viewModel)
        binding.cheatList.layoutManager = LinearLayoutManager(activity)

        val divider = SettingsDividerItemDecoration(requireActivity())
        binding.cheatList.addItemDecoration(divider)

        @ColorInt val color =
            ElevationOverlayProvider(binding.cheatsWarning.context).compositeOverlay(
                MaterialColors.getColor(binding.cheatsWarning, R.attr.colorSurface),
                resources.getDimensionPixelSize(R.dimen.elevated_app_bar).toFloat()
            )
        binding.cheatsWarning.setBackgroundColor(color)
        binding.gfxModsWarning.setBackgroundColor(color)

        setInsets()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(binding.cheatList) { v: View, windowInsets: WindowInsetsCompat ->
            val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            v.setPadding(
                0,
                0,
                0,
                insets.bottom + resources.getDimensionPixelSize(R.dimen.spacing_xtralarge)
            )
            windowInsets
        }
    }
}
