// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui

import android.content.Context
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentActivity
import androidx.recyclerview.widget.LinearLayoutManager
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.databinding.FragmentSettingsBinding
import org.dolphinemu.dolphinemu.features.settings.model.Settings
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem
import org.dolphinemu.dolphinemu.utils.SerializableHelper.serializable
import java.util.*
import kotlin.collections.ArrayList

class SettingsFragment : Fragment(), SettingsFragmentView {
    private lateinit var presenter: SettingsFragmentPresenter
    private var activityView: SettingsActivityView? = null

    private lateinit var menuTag: MenuTag

    override val fragmentActivity: FragmentActivity
        get() = requireActivity()

    override var adapter: SettingsAdapter? = null

    private var oldControllerSettingsWarningHeight = 0

    private var binding: FragmentSettingsBinding? = null

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
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        binding = FragmentSettingsBinding.inflate(inflater, container, false)
        return binding!!.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        if (titles.containsKey(menuTag)) {
            activityView!!.setToolbarTitle(getString(titles[menuTag]!!))
        }

        val manager = LinearLayoutManager(activity)

        val recyclerView = binding!!.listSettings
        recyclerView.adapter = adapter
        recyclerView.layoutManager = manager

        val divider = SettingsDividerItemDecoration(requireActivity())
        recyclerView.addItemDecoration(divider)

        setInsets()

        val activity = activity as SettingsActivityView?
        presenter.onViewCreated(menuTag, activity!!.settings)
    }

    override fun onDestroyView() {
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
        adapter!!.setSettings(settingsList)
    }

    override fun loadDefaultSettings() {
        presenter.loadDefaultSettings()
    }

    override fun loadSubMenu(menuKey: MenuTag) {
        activityView!!.showSettingsFragment(
            menuKey,
            null,
            true,
            requireArguments().getString(ARGUMENT_GAME_ID)!!
        )
    }

    override fun showDialogFragment(fragment: DialogFragment) {
        activityView!!.showDialogFragment(fragment)
    }

    override fun showToastMessage(message: String) {
        activityView!!.showToastMessage(message)
    }

    override val settings: Settings?
        get() = presenter.settings

    override fun onSettingChanged() {
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

    companion object {
        private const val ARGUMENT_MENU_TAG = "menu_tag"
        private const val ARGUMENT_GAME_ID = "game_id"
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
            titles[MenuTag.CONFIG_ADVANCED] = R.string.advanced_submenu
            titles[MenuTag.DEBUG] = R.string.debug_submenu
            titles[MenuTag.GRAPHICS] = R.string.graphics_settings
            titles[MenuTag.ENHANCEMENTS] = R.string.enhancements_submenu
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
