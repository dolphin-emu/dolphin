// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import org.dolphinemu.dolphinemu.databinding.FragmentCheatWarningBinding
import org.dolphinemu.dolphinemu.features.cheats.ui.CheatsActivity.Companion.setOnFocusChangeListenerRecursively
import org.dolphinemu.dolphinemu.features.settings.model.AbstractBooleanSetting
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivity

abstract class SettingDisabledWarningFragment(
    private val setting: AbstractBooleanSetting,
    private val settingShortcut: MenuTag,
    private val text: Int
) : Fragment(), View.OnClickListener {
    private var _binding: FragmentCheatWarningBinding? = null
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        _binding = FragmentCheatWarningBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        binding.textWarning.setText(text)
        binding.buttonSettings.setOnClickListener(this)
        val activity = requireActivity() as CheatsActivity
        setOnFocusChangeListenerRecursively(view) { _: View?, hasFocus: Boolean ->
            activity.onListViewFocusChange(hasFocus)
        }
    }

    override fun onResume() {
        super.onResume()
        val activity = requireActivity() as CheatsActivity
        activity.loadGameSpecificSettings().use {
            val cheatsEnabled = setting.boolean
            requireView().visibility = if (cheatsEnabled) View.GONE else View.VISIBLE
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    override fun onClick(view: View) {
        SettingsActivity.launch(requireContext(), settingShortcut)
    }
}
