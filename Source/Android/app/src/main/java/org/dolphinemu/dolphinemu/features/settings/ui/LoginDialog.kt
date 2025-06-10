// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.launch
import org.dolphinemu.dolphinemu.databinding.DialogLoginBinding
import org.dolphinemu.dolphinemu.dialogs.AlertMessage
import org.dolphinemu.dolphinemu.features.settings.model.AchievementModel.asyncLogin
import org.dolphinemu.dolphinemu.features.settings.model.NativeConfig
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting

class LoginDialog(val parent: SettingsFragmentPresenter) : DialogFragment() {
    private var _binding: DialogLoginBinding? = null
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = DialogLoginBinding.inflate(inflater, container, false)
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
        binding.loginFailed.visibility = View.GONE
        binding.loginInProgress.visibility = View.VISIBLE
        StringSetting.ACHIEVEMENTS_USERNAME.setString(NativeConfig.LAYER_BASE_OR_CURRENT,
            binding.usernameInput.text.toString())
        lifecycleScope.launch {
            if (asyncLogin(binding.passwordInput.text.toString())) {
              parent.loadSettingsList()
              dismiss()
            } else {
              binding.loginInProgress.visibility = View.GONE
              binding.loginFailed.visibility = View.VISIBLE
            }
        }
    }
}
