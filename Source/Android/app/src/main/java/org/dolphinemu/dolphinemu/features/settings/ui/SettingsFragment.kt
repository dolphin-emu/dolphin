// SPDX-License-Identifier: GPL-2.0-or-later

// GPU driver implementation partially based on:
// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui

import android.app.Activity
import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.activity.result.ActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentActivity
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.snackbar.Snackbar
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.databinding.FragmentSettingsBinding
import org.dolphinemu.dolphinemu.features.settings.model.Settings
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem
import org.dolphinemu.dolphinemu.features.settings.ui.viewholder.SettingViewHolder
import org.dolphinemu.dolphinemu.utils.GpuDriverInstallResult
import org.dolphinemu.dolphinemu.utils.SerializableHelper.serializable
import java.util.EnumMap

class SettingsFragment : Fragment(), SettingsFragmentView {
    private lateinit var presenter: SettingsFragmentPresenter
    private var activityView: SettingsActivityView? = null

    private lateinit var menuTag: MenuTag

    override val fragmentActivity: FragmentActivity
        get() = requireActivity()

    override var adapter: SettingsAdapter? = null

    override val activityResultLaunchers: SettingsActivityResultLaunchers =
        SettingsActivityResultLaunchers(this) { adapter }

    private var oldControllerSettingsWarningHeight = 0
    private var hasScrolledToSearchResult = false
    private var highlightedSearchResult: SettingsItem? = null
    private var highlightedSearchResultPosition = RecyclerView.NO_POSITION
    private var searchIndexWarmupJob: Job? = null
    private var searchJob: Job? = null

    private var binding: FragmentSettingsBinding? = null

    private val requestGpuDriver = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { result: ActivityResult ->
        val uri = result.data?.data
        if (result.resultCode == Activity.RESULT_OK && uri != null) {
            presenter.installDriver(uri)
        }
    }

    override fun onAttach(context: Context) {
        super.onAttach(context)

        activityView = context as SettingsActivityView
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        menuTag = requireArguments().serializable(ARGUMENT_MENU_TAG)!!

        val gameId = requireArguments().getString(ARGUMENT_GAME_ID)
        presenter = SettingsFragmentPresenter(this, requireContext())
        adapter = SettingsAdapter(this, requireContext())

        presenter.onCreate(menuTag, gameId, requireArguments())
    }

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?
    ): View {
        binding = FragmentSettingsBinding.inflate(inflater, container, false)
        return binding!!.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        if (titles.containsKey(menuTag)) {
            activityView!!.setToolbarState(
                getString(titles[menuTag]!!),
                menuTag != MenuTag.SETTINGS,
                menuTag == MenuTag.SETTINGS
            )
        }

        val manager = LinearLayoutManager(activity)

        val recyclerView = binding!!.listSettings
        recyclerView.adapter = adapter
        recyclerView.layoutManager = manager

        val divider = SettingsDividerItemDecoration(requireActivity())
        recyclerView.addItemDecoration(divider)

        setInsets()

        val activity = requireActivity() as SettingsActivityView
        presenter.invalidateSearchIndex()
        presenter.onViewCreated(menuTag, activity.settings)
    }

    override fun onDestroyView() {
        clearSearchResultHighlight()
        searchJob?.cancel()
        super.onDestroyView()
        binding = null
    }

    override fun onDetach() {
        super.onDetach()
        activityView = null

        if (adapter != null) {
            adapter!!.closeDialog()
        }
    }

    override fun onSettingsFileLoaded(settings: Settings) {
        presenter.settings = settings
    }

    override fun showSettingsList(settingsList: ArrayList<SettingsItem>) {
        val query = activityView?.settingsSearchQuery.orEmpty()
        val isShowingSearch =
            menuTag == MenuTag.SETTINGS && activityView?.isSettingsSearchActive == true
        if (!isShowingSearch) {
            adapter!!.setSettings(settingsList)
        }
        if (menuTag == MenuTag.SETTINGS) {
            warmUpSearchIndex()
            if (isShowingSearch) {
                applySettingsFilter(query)
            }
        }

        val position = arguments?.getInt(
            ARGUMENT_SCROLL_TO_SETTING_POSITION, RecyclerView.NO_POSITION
        ) ?: RecyclerView.NO_POSITION
        if (!hasScrolledToSearchResult && position in settingsList.indices) {
            hasScrolledToSearchResult = true
            binding?.listSettings?.post {
                val recyclerView = binding?.listSettings ?: return@post
                (recyclerView.layoutManager as? LinearLayoutManager)?.scrollToPositionWithOffset(
                    position, 0
                )
                highlightSearchResult(position, settingsList[position])
            }
        }
    }

    fun filterSettings(query: String) {
        if (!this::presenter.isInitialized || presenter.settings == null) {
            return
        }

        applySettingsFilter(query)
    }

    private fun applySettingsFilter(query: String) {
        searchJob?.cancel()
        if (query.isBlank()) {
            val results = if (activityView?.isSettingsSearchActive == true) {
                arrayListOf()
            } else {
                presenter.getSettingsList()
            }
            showSearchResults(query, results)
            return
        }

        searchJob = viewLifecycleOwner.lifecycleScope.launch {
            delay(SEARCH_QUERY_DEBOUNCE_MS)
            val results = presenter.searchSettings(query)
            if (activityView?.settingsSearchQuery == query) {
                showSearchResults(query, results)
            }
        }
    }

    private fun warmUpSearchIndex() {
        if (searchIndexWarmupJob?.isActive == true) {
            return
        }

        searchIndexWarmupJob = viewLifecycleOwner.lifecycleScope.launch {
            presenter.prepareSearchIndex()
        }
    }

    private fun showSearchResults(query: String, results: ArrayList<SettingsItem>) {
        adapter!!.setSettings(results)
        binding?.textNoSearchResults?.text =
            getString(R.string.search_settings_no_results, query.trim())
        binding?.textNoSearchResults?.visibility =
            if (query.isNotBlank() && results.isEmpty()) View.VISIBLE else View.GONE
        binding?.listSettings?.visibility =
            if (query.isNotBlank() && results.isEmpty()) View.GONE else View.VISIBLE
    }

    override fun loadSubMenu(menuKey: MenuTag) {
        if (menuKey == MenuTag.GPU_DRIVERS) {
            showGpuDriverDialog()
            return
        }

        activityView!!.showSettingsFragment(
            menuKey, null, true, requireArguments().getString(ARGUMENT_GAME_ID)!!
        )
    }

    override fun loadSearchResult(menuKey: MenuTag, settingPosition: Int, extras: Bundle?) {
        activityView!!.showSearchResult(
            menuKey, settingPosition, requireArguments().getString(ARGUMENT_GAME_ID)!!, extras
        )
    }

    private fun highlightSearchResult(position: Int, setting: SettingsItem) {
        val recyclerView = binding?.listSettings ?: return
        highlightedSearchResult = setting
        highlightedSearchResultPosition = position
        recyclerView.post {
            (recyclerView.findViewHolderForAdapterPosition(position) as? SettingViewHolder)
                ?.highlightSearchResult()
        }
    }

    private fun clearSearchResultHighlight() {
        val recyclerView = binding?.listSettings
        (recyclerView?.findViewHolderForAdapterPosition(
            highlightedSearchResultPosition
        ) as? SettingViewHolder)?.clearSearchResultHighlight()
        highlightedSearchResult = null
        highlightedSearchResultPosition = RecyclerView.NO_POSITION
    }

    override fun showDialogFragment(fragment: DialogFragment) {
        activityView!!.showDialogFragment(fragment)
    }

    override fun showToastMessage(message: String) {
        activityView!!.showToastMessage(message)
    }

    override val settings: Settings?
        get() = presenter.settings

    override fun onSettingChanged(setting: SettingsItem?) {
        if (setting == null || setting === highlightedSearchResult) {
            clearSearchResultHighlight()
        }
        presenter.invalidateSearchIndex()
        activityView!!.onSettingChanged()
    }

    override fun onControllerSettingsChanged() {
        adapter!!.notifyAllSettingsChanged()
        presenter.updateOldControllerSettingsWarningVisibility()
    }

    override fun onMenuTagAction(menuTag: MenuTag, value: Int) {
        activityView!!.onMenuTagAction(menuTag, value)
    }

    override fun hasMenuTagActionForValue(menuTag: MenuTag, value: Int): Boolean {
        return activityView!!.hasMenuTagActionForValue(menuTag, value)
    }

    override fun getMenuTagActionExtras(menuTag: MenuTag, value: Int): Bundle? {
        return activityView!!.getMenuTagActionExtras(menuTag, value)
    }

    override var isMappingAllDevices: Boolean
        get() = activityView!!.isMappingAllDevices
        set(allDevices) {
            activityView!!.isMappingAllDevices = allDevices
        }

    override fun setOldControllerSettingsWarningVisibility(visible: Boolean) {
        oldControllerSettingsWarningHeight =
            activityView!!.setOldControllerSettingsWarningVisibility(visible)

        // Trigger the insets listener we've registered
        binding!!.listSettings.requestApplyInsets()
    }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(binding!!.listSettings) { v: View, windowInsets: WindowInsetsCompat ->
            val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val listSpacing = resources.getDimensionPixelSize(R.dimen.spacing_list)
            v.updatePadding(bottom = insets.bottom + listSpacing + oldControllerSettingsWarningHeight)
            windowInsets
        }
    }

    override fun showGpuDriverDialog() {
        if (presenter.gpuDriver == null) {
            return
        }
        val msg = "${presenter.gpuDriver!!.name} ${presenter.gpuDriver!!.driverVersion}"

        MaterialAlertDialogBuilder(requireContext()).setTitle(getString(R.string.gpu_driver_dialog_title))
            .setMessage(msg).setNegativeButton(android.R.string.cancel, null)
            .setNeutralButton(R.string.gpu_driver_dialog_system) { _: DialogInterface?, _: Int ->
                presenter.useSystemDriver()
            }.setPositiveButton(R.string.gpu_driver_dialog_install) { _: DialogInterface?, _: Int ->
                askForDriverFile()
            }.show()
    }

    override fun getFragmentLifecycle(): Lifecycle {
        return lifecycle
    }

    private fun askForDriverFile() {
        val intent = Intent(Intent.ACTION_GET_CONTENT).apply {
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            type = "application/zip"
        }
        requestGpuDriver.launch(intent)
    }

    override fun onDriverInstallDone(result: GpuDriverInstallResult) {
        val view = binding?.root ?: return
        Snackbar.make(view, resolveInstallResultString(result), Snackbar.LENGTH_LONG).show()
    }

    override fun onDriverUninstallDone() {
        Toast.makeText(
            requireContext(), R.string.gpu_driver_dialog_uninstall_done, Toast.LENGTH_SHORT
        ).show()
    }

    private fun resolveInstallResultString(result: GpuDriverInstallResult) = when (result) {
        GpuDriverInstallResult.Success -> getString(R.string.gpu_driver_install_success)
        GpuDriverInstallResult.InvalidArchive -> getString(R.string.gpu_driver_install_invalid_archive)
        GpuDriverInstallResult.MissingMetadata -> getString(R.string.gpu_driver_install_missing_metadata)
        GpuDriverInstallResult.InvalidMetadata -> getString(R.string.gpu_driver_install_invalid_metadata)
        GpuDriverInstallResult.UnsupportedAndroidVersion -> getString(R.string.gpu_driver_install_unsupported_android_version)
        GpuDriverInstallResult.AlreadyInstalled -> getString(R.string.gpu_driver_install_already_installed)
        GpuDriverInstallResult.FileNotFound -> getString(R.string.gpu_driver_install_file_not_found)
    }

    companion object {
        private const val ARGUMENT_MENU_TAG = "menu_tag"
        private const val ARGUMENT_GAME_ID = "game_id"
        const val ARGUMENT_SCROLL_TO_SETTING_POSITION = "scroll_to_setting_position"
        private const val SEARCH_QUERY_DEBOUNCE_MS = 120L
        private val titles: MutableMap<MenuTag, Int> = EnumMap(MenuTag::class.java)

        init {
            titles[MenuTag.SETTINGS] = R.string.settings
            titles[MenuTag.CONFIG] = R.string.config
            titles[MenuTag.CONFIG_GENERAL] = R.string.general_submenu
            titles[MenuTag.CONFIG_INTERFACE] = R.string.interface_submenu
            titles[MenuTag.CONFIG_AUDIO] = R.string.audio_submenu
            titles[MenuTag.CONFIG_PATHS] = R.string.paths_submenu
            titles[MenuTag.CONFIG_GAME_CUBE] = R.string.gamecube_submenu
            titles[MenuTag.CONFIG_SERIALPORT1] = R.string.serialport1_submenu
            titles[MenuTag.CONFIG_WII] = R.string.wii_submenu
            titles[MenuTag.CONFIG_ACHIEVEMENTS] = R.string.achievements_submenu
            titles[MenuTag.CONFIG_ADVANCED] = R.string.advanced_submenu
            titles[MenuTag.DEBUG] = R.string.debug_submenu
            titles[MenuTag.GRAPHICS] = R.string.graphics_settings
            titles[MenuTag.ENHANCEMENTS] = R.string.enhancements_submenu
            titles[MenuTag.COLOR_CORRECTION] = R.string.color_correction_submenu
            titles[MenuTag.STEREOSCOPY] = R.string.stereoscopy_submenu
            titles[MenuTag.HACKS] = R.string.hacks_submenu
            titles[MenuTag.STATISTICS] = R.string.statistics_submenu
            titles[MenuTag.ADVANCED_GRAPHICS] = R.string.advanced_graphics_submenu
            titles[MenuTag.CONFIG_LOG] = R.string.log_submenu
            titles[MenuTag.GCPAD_TYPE] = R.string.gcpad_settings
            titles[MenuTag.WIIMOTE] = R.string.wiimote_settings
            titles[MenuTag.WIIMOTE_EXTENSION] = R.string.wiimote_extensions
            titles[MenuTag.GCPAD_1] = R.string.controller_0
            titles[MenuTag.GCPAD_2] = R.string.controller_1
            titles[MenuTag.GCPAD_3] = R.string.controller_2
            titles[MenuTag.GCPAD_4] = R.string.controller_3
            titles[MenuTag.WIIMOTE_1] = R.string.wiimote_0
            titles[MenuTag.WIIMOTE_2] = R.string.wiimote_1
            titles[MenuTag.WIIMOTE_3] = R.string.wiimote_2
            titles[MenuTag.WIIMOTE_4] = R.string.wiimote_3
            titles[MenuTag.WIIMOTE_EXTENSION_1] = R.string.wiimote_extension_0
            titles[MenuTag.WIIMOTE_EXTENSION_2] = R.string.wiimote_extension_1
            titles[MenuTag.WIIMOTE_EXTENSION_3] = R.string.wiimote_extension_2
            titles[MenuTag.WIIMOTE_EXTENSION_4] = R.string.wiimote_extension_3
            titles[MenuTag.WIIMOTE_GENERAL_1] = R.string.wiimote_general
            titles[MenuTag.WIIMOTE_GENERAL_2] = R.string.wiimote_general
            titles[MenuTag.WIIMOTE_GENERAL_3] = R.string.wiimote_general
            titles[MenuTag.WIIMOTE_GENERAL_4] = R.string.wiimote_general
            titles[MenuTag.WIIMOTE_MOTION_SIMULATION_1] = R.string.wiimote_motion_simulation
            titles[MenuTag.WIIMOTE_MOTION_SIMULATION_2] = R.string.wiimote_motion_simulation
            titles[MenuTag.WIIMOTE_MOTION_SIMULATION_3] = R.string.wiimote_motion_simulation
            titles[MenuTag.WIIMOTE_MOTION_SIMULATION_4] = R.string.wiimote_motion_simulation
            titles[MenuTag.WIIMOTE_MOTION_INPUT_1] = R.string.wiimote_motion_input
            titles[MenuTag.WIIMOTE_MOTION_INPUT_2] = R.string.wiimote_motion_input
            titles[MenuTag.WIIMOTE_MOTION_INPUT_3] = R.string.wiimote_motion_input
            titles[MenuTag.WIIMOTE_MOTION_INPUT_4] = R.string.wiimote_motion_input
        }

        @JvmStatic
        fun newInstance(menuTag: MenuTag?, gameId: String?, extras: Bundle?): Fragment {
            val fragment = SettingsFragment()

            val arguments = Bundle()
            if (extras != null) {
                arguments.putAll(extras)
            }

            arguments.putSerializable(ARGUMENT_MENU_TAG, menuTag)
            arguments.putString(ARGUMENT_GAME_ID, gameId)

            fragment.arguments = arguments
            return fragment
        }
    }
}
