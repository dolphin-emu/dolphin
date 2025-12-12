// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.achievements.ui

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModelProvider
import androidx.recyclerview.widget.LinearLayoutManager
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.databinding.FragmentAchievementsListBinding
import org.dolphinemu.dolphinemu.features.achievements.model.AchievementProgressViewModel
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsDividerItemDecoration

class AchievementProgressListFragment : Fragment() {
    private var _binding: FragmentAchievementsListBinding? = null
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentAchievementsListBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        val activity = requireActivity() as AchievementsActivity
        val viewModel = ViewModelProvider(activity)[AchievementProgressViewModel::class.java]

        binding.achievementsList.adapter = AchievementProgressAdapter(activity, viewModel)
        binding.achievementsList.layoutManager = LinearLayoutManager(activity)

        val divider = SettingsDividerItemDecoration(requireActivity())
        binding.achievementsList.addItemDecoration(divider)

        setInsets()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(binding.achievementsList) { v: View, windowInsets: WindowInsetsCompat ->
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
