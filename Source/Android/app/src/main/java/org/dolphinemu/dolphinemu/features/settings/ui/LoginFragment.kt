// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.DialogFragment
import org.dolphinemu.dolphinemu.databinding.FragmentLoginBinding
import org.dolphinemu.dolphinemu.features.settings.model.AchievementModel.login
import org.dolphinemu.dolphinemu.features.settings.model.NativeConfig
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting

class LoginFragment : DialogFragment() {
    private var _binding: FragmentLoginBinding? = null
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentLoginBinding.inflate(inflater, container, false)
        binding.usernameInput.setText(
            StringSetting.ACHIEVEMENTS_USERNAME.string
        )
        return binding.getRoot()
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        binding.buttonCancel.setOnClickListener { onCancelClicked() }
        binding.buttonLogin.setOnClickListener { onLoginClicked() }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private fun onCancelClicked() {
        dismiss()
    }

    private fun onLoginClicked() {
        StringSetting.ACHIEVEMENTS_USERNAME.setString(NativeConfig.LAYER_BASE_OR_CURRENT,
            binding.usernameInput.text.toString())
        login(binding.passwordInput.text.toString())
        dismiss()
    }
}
