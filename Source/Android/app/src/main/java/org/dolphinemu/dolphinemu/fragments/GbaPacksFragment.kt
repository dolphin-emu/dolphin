// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.fragments

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.activities.EmulationActivity
import org.dolphinemu.dolphinemu.databinding.FragmentGbaPacksBinding
import org.dolphinemu.dolphinemu.features.dualscreen.GbaBootManager
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting

class GbaPacksFragment : Fragment() {
    private var _binding: FragmentGbaPacksBinding? = null
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentGbaPacksBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        binding.gbaPackPlayer.text = getString(
            R.string.emulation_gba_pack_player_with_value,
            titleForGameBoyPlayer()
        )
        binding.gbaPackPlayer.setOnClickListener {
            (requireActivity() as EmulationActivity).chooseGameBoyPlayerRom()
        }
        binding.layoutGbaPacks.requestFocus()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private fun titleForGameBoyPlayer(): String {
        val path = StringSetting.MAIN_GB_PLAYER_ROM.string
        return if (path.isEmpty()) {
            getString(R.string.emulation_gba_pack_empty)
        } else {
            GbaBootManager.titleFor(path)
        }
    }
}
