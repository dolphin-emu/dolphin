// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.fragments

import android.content.pm.PackageManager
import android.os.Bundle
import android.util.SparseIntArray
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import androidx.annotation.ColorInt
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.fragment.app.Fragment
import com.google.android.material.color.MaterialColors
import com.google.android.material.elevation.ElevationOverlayProvider
import org.dolphinemu.dolphinemu.NativeLibrary
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.activities.EmulationActivity
import org.dolphinemu.dolphinemu.databinding.FragmentIngameMenuBinding
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting
import org.dolphinemu.dolphinemu.utils.InsetsHelper
import org.dolphinemu.dolphinemu.utils.ThemeHelper

class MenuFragment : Fragment(), View.OnClickListener {
    private var cutInset = 0

    private var _binding: FragmentIngameMenuBinding? = null
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentIngameMenuBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        if (IntSetting.MAIN_INTERFACE_THEME.int != ThemeHelper.DEFAULT) {
            @ColorInt val color = ElevationOverlayProvider(view.context).compositeOverlay(
                MaterialColors.getColor(view, R.attr.colorSurface),
                view.elevation
            )
            view.setBackgroundColor(color)
        }

        setInsets()
        updatePauseUnpauseVisibility()

        if (!requireActivity().packageManager.hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN)) {
            binding.menuOverlayControls.visibility = View.GONE
        }

        if (!requireArguments().getBoolean(KEY_WII, true)) {
            binding.menuRefreshWiimotes.visibility = View.GONE
            binding.menuSkylanders.visibility = View.GONE
            binding.menuInfinityBase.visibility = View.GONE
        }

        if (!BooleanSetting.MAIN_EMULATE_SKYLANDER_PORTAL.boolean) {
            binding.menuSkylanders.visibility = View.GONE
        }

        if (!BooleanSetting.MAIN_EMULATE_INFINITY_BASE.boolean) {
            binding.menuInfinityBase.visibility = View.GONE
        }

        val options = binding.layoutOptions
        for (childIndex in 0 until options.childCount) {
            val button = options.getChildAt(childIndex) as Button
            button.setOnClickListener(this)
        }

        binding.menuExit.setOnClickListener(this)

        val title = requireArguments().getString(KEY_TITLE, null)
        if (title != null) {
            binding.textGameTitle.text = title
        }
    }

    private fun setInsets() =
        ViewCompat.setOnApplyWindowInsetsListener(binding.root) { v: View, windowInsets: WindowInsetsCompat ->
            val cutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())
            cutInset = cutInsets.left
            var left = 0
            var right = 0
            if (ViewCompat.getLayoutDirection(v) == ViewCompat.LAYOUT_DIRECTION_LTR) {
                left = cutInsets.left
            } else {
                right = cutInsets.right
            }
            v.post { NativeLibrary.SetObscuredPixelsLeft(v.width) }

            // Don't use padding if the navigation bar isn't in the way
            if (InsetsHelper.getBottomPaddingRequired(requireActivity()) > 0) {
                v.setPadding(
                    left, cutInsets.top, right,
                    cutInsets.bottom + InsetsHelper.getNavigationBarHeight(requireContext())
                )
            } else {
                v.setPadding(
                    left, cutInsets.top, right,
                    cutInsets.bottom + resources.getDimensionPixelSize(R.dimen.spacing_large)
                )
            }
            windowInsets
        }

    override fun onResume() {
        super.onResume()
        val savestatesEnabled = BooleanSetting.MAIN_ENABLE_SAVESTATES.boolean
        val savestateVisibility = if (savestatesEnabled) View.VISIBLE else View.GONE
        binding.menuQuicksave.visibility = savestateVisibility
        binding.menuQuickload.visibility = savestateVisibility
        binding.menuEmulationSaveRoot.visibility = savestateVisibility
        binding.menuEmulationLoadRoot.visibility = savestateVisibility
    }

    override fun onDestroyView() {
        super.onDestroyView()
        NativeLibrary.SetObscuredPixelsLeft(cutInset)
        _binding = null
    }

    private fun updatePauseUnpauseVisibility() {
        val paused = EmulationActivity.hasUserPausedEmulation
        binding.menuUnpauseEmulation.visibility = if (paused) View.VISIBLE else View.GONE
        binding.menuPauseEmulation.visibility = if (paused) View.GONE else View.VISIBLE
    }

    override fun onClick(button: View) {
        val action = buttonsActionsMap[button.id]
        val activity = requireActivity() as EmulationActivity

        if (action == EmulationActivity.MENU_ACTION_OVERLAY_CONTROLS) {
            // We could use the button parameter as the anchor here, but this often results in a tiny menu
            // (because the button often is in the middle of the screen), so let's use mTitleText instead
            activity.showOverlayControlsMenu(binding.textGameTitle)
        } else if (action >= 0) {
            activity.handleMenuAction(action)
        }

        if (action == EmulationActivity.MENU_ACTION_PAUSE_EMULATION ||
            action == EmulationActivity.MENU_ACTION_UNPAUSE_EMULATION
        ) {
            updatePauseUnpauseVisibility()
        }
    }

    companion object {
        private const val KEY_TITLE = "title"
        private const val KEY_WII = "wii"
        private val buttonsActionsMap = SparseIntArray()

        init {
            buttonsActionsMap.append(
                R.id.menu_pause_emulation,
                EmulationActivity.MENU_ACTION_PAUSE_EMULATION
            )
            buttonsActionsMap.append(
                R.id.menu_unpause_emulation,
                EmulationActivity.MENU_ACTION_UNPAUSE_EMULATION
            )
            buttonsActionsMap.append(
                R.id.menu_take_screenshot,
                EmulationActivity.MENU_ACTION_TAKE_SCREENSHOT
            )
            buttonsActionsMap.append(R.id.menu_quicksave, EmulationActivity.MENU_ACTION_QUICK_SAVE)
            buttonsActionsMap.append(R.id.menu_quickload, EmulationActivity.MENU_ACTION_QUICK_LOAD)
            buttonsActionsMap.append(
                R.id.menu_emulation_save_root,
                EmulationActivity.MENU_ACTION_SAVE_ROOT
            )
            buttonsActionsMap.append(
                R.id.menu_emulation_load_root,
                EmulationActivity.MENU_ACTION_LOAD_ROOT
            )
            buttonsActionsMap.append(
                R.id.menu_overlay_controls,
                EmulationActivity.MENU_ACTION_OVERLAY_CONTROLS
            )
            buttonsActionsMap.append(
                R.id.menu_refresh_wiimotes,
                EmulationActivity.MENU_ACTION_REFRESH_WIIMOTES
            )
            buttonsActionsMap.append(
                R.id.menu_change_disc,
                EmulationActivity.MENU_ACTION_CHANGE_DISC
            )
            buttonsActionsMap.append(R.id.menu_exit, EmulationActivity.MENU_ACTION_EXIT)
            buttonsActionsMap.append(R.id.menu_settings, EmulationActivity.MENU_ACTION_SETTINGS)
            buttonsActionsMap.append(R.id.menu_skylanders, EmulationActivity.MENU_ACTION_SKYLANDERS)
            buttonsActionsMap.append(
                R.id.menu_infinity_base,
                EmulationActivity.MENU_ACTION_INFINITY_BASE
            )
        }

        fun newInstance(): MenuFragment {
            val fragment = MenuFragment()
            val arguments = Bundle()
            if (NativeLibrary.IsGameMetadataValid()) {
                arguments.putString(KEY_TITLE, NativeLibrary.GetCurrentTitleDescription())
                arguments.putBoolean(KEY_WII, NativeLibrary.IsEmulatingWii())
            }
            fragment.arguments = arguments
            return fragment
        }
    }
}
