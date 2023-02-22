// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.fragments

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModelProvider
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.activities.EmulationActivity
import org.dolphinemu.dolphinemu.databinding.FragmentMoreBinding
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting
import org.dolphinemu.dolphinemu.features.settings.model.NativeConfig
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag
import org.dolphinemu.dolphinemu.features.sysupdate.ui.SystemMenuNotInstalledDialogFragment
import org.dolphinemu.dolphinemu.features.sysupdate.ui.SystemUpdateProgressBarDialogFragment
import org.dolphinemu.dolphinemu.features.sysupdate.ui.SystemUpdateViewModel
import org.dolphinemu.dolphinemu.services.GameFileCacheManager
import org.dolphinemu.dolphinemu.ui.Tab
import org.dolphinemu.dolphinemu.ui.main.MainActivityViewModel
import org.dolphinemu.dolphinemu.ui.main.MainPresenter
import org.dolphinemu.dolphinemu.ui.main.MainView
import org.dolphinemu.dolphinemu.utils.AfterDirectoryInitializationRunner
import org.dolphinemu.dolphinemu.utils.ThemeHelper
import org.dolphinemu.dolphinemu.utils.WiiUtils

class MoreFragment : Fragment() {
    private var _binding: FragmentMoreBinding? = null
    private val binding get() = _binding!!

    private lateinit var mainActivityViewModel: MainActivityViewModel
    private lateinit var systemUpDateViewModel: SystemUpdateViewModel

    private lateinit var mainView: MainView

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentMoreBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        mainActivityViewModel =
            ViewModelProvider(requireActivity())[MainActivityViewModel::class.java]
        systemUpDateViewModel =
            ViewModelProvider(requireActivity())[SystemUpdateViewModel::class.java]
        mainView = requireActivity() as MainView

        if (resources.getBoolean(R.bool.smallLayout)) {
            mainActivityViewModel.setFabVisible(false)
        }

        AfterDirectoryInitializationRunner().runWithLifecycle(requireActivity()) {
            IntSetting.MAIN_LAST_PLATFORM_TAB.setInt(
                NativeConfig.LAYER_BASE,
                Tab.MORE.toInt()
            )
        }

        systemUpDateViewModel.resultData.observe(viewLifecycleOwner) {
            if (WiiUtils.isSystemMenuInstalled()) {
                val resId =
                    if (WiiUtils.isSystemMenuvWii()) R.string.load_vwii_system_menu_installed else R.string.load_wii_system_menu_installed
                binding.loadWiiSystemMenuText.text =
                    getString(resId, WiiUtils.getSystemMenuVersion())
            }
        }

        ThemeHelper.setStatusBarColor(
            requireActivity() as AppCompatActivity,
            ContextCompat.getColor(requireContext(), android.R.color.transparent)
        )

        setUpButtons()
        setInsets()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private fun setUpButtons() {
        binding.optionOpenFile.setOnClickListener {
            mainView.launchOpenFileActivity(MainPresenter.REQUEST_GAME_FILE)
        }
        binding.optionInstallWad.setOnClickListener {
            mainView.launchOpenFileActivity(MainPresenter.REQUEST_GAME_FILE)
        }
        binding.optionLoadWiiSystemMenu.setOnClickListener {
            launchWiiSystemMenu()
        }
        binding.optionImportWiiSave.setOnClickListener {
            AfterDirectoryInitializationRunner().runWithLifecycle(activity) {
                mainView.launchOpenFileActivity(
                    MainPresenter.REQUEST_WII_SAVE_FILE
                )
            }
        }
        binding.optionImportNandBackup.setOnClickListener {
            AfterDirectoryInitializationRunner().runWithLifecycle(activity) {
                mainView.launchOpenFileActivity(
                    MainPresenter.REQUEST_NAND_BIN_FILE
                )
            }
        }
        binding.optionOnlineSystemUpdate.setOnClickListener {
            AfterDirectoryInitializationRunner().runWithLifecycle(activity) {
                launchOnlineUpdate()
            }
        }
        binding.optionRefresh.setOnClickListener {
            GameFileCacheManager.startRescan()
            Toast.makeText(
                context,
                resources.getString(R.string.refreshing_library),
                Toast.LENGTH_SHORT
            ).show()
        }
        binding.optionSettings.setOnClickListener {
            mainView.launchSettingsActivity(MenuTag.SETTINGS)
        }
        binding.optionAbout.setOnClickListener {
            AboutDialogFragment().show(
                requireActivity().supportFragmentManager,
                AboutDialogFragment.TAG
            )
        }
    }

    private fun launchWiiSystemMenu() {
        AfterDirectoryInitializationRunner().runWithLifecycle(requireActivity()) {
            if (WiiUtils.isSystemMenuInstalled()) {
                EmulationActivity.launchSystemMenu(requireActivity())
            } else {
                val dialogFragment =
                    SystemMenuNotInstalledDialogFragment()
                dialogFragment
                    .show(
                        requireActivity().supportFragmentManager,
                        "SystemMenuNotInstalledDialogFragment"
                    )
            }
        }
    }

    private fun launchOnlineUpdate() {
        if (WiiUtils.isSystemMenuInstalled()) {
            systemUpDateViewModel.region = -1
            launchUpdateProgressBarFragment()
        } else {
            SystemMenuNotInstalledDialogFragment().show(
                requireActivity().supportFragmentManager,
                "SystemMenuNotInstalledDialogFragment"
            )
        }
    }

    private fun launchUpdateProgressBarFragment() {
        val progressBarFragment = SystemUpdateProgressBarDialogFragment()
        progressBarFragment.show(
            requireActivity().supportFragmentManager,
            "SystemUpdateProgressBarDialogFragment"
        )
        progressBarFragment.isCancelable = false
    }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(binding.moreList) { _: View, windowInsets: WindowInsetsCompat ->
            val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            if (resources.getBoolean(R.bool.smallLayout)) {
                binding.moreList.setPadding(
                    insets.left,
                    insets.top,
                    insets.right,
                    insets.bottom + resources.getDimensionPixelSize(R.dimen.spacing_navigation)
                )
            } else {
                binding.moreList.setPadding(
                    insets.left,
                    insets.top,
                    insets.right,
                    insets.bottom
                )
            }
            windowInsets
        }
    }
}
