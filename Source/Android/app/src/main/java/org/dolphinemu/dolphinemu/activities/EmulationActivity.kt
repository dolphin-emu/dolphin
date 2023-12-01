// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.activities

import android.annotation.SuppressLint
import android.content.DialogInterface
import android.content.Intent
import android.graphics.Rect
import android.os.Build
import android.os.Bundle
import android.util.SparseIntArray
import android.view.KeyEvent
import android.view.MenuItem
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup.MarginLayoutParams
import android.view.WindowManager
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.PopupMenu
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentActivity
import androidx.fragment.app.FragmentManager
import androidx.recyclerview.widget.LinearLayoutManager
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.slider.Slider
import org.dolphinemu.dolphinemu.NativeLibrary
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.databinding.ActivityEmulationBinding
import org.dolphinemu.dolphinemu.databinding.DialogInputAdjustBinding
import org.dolphinemu.dolphinemu.databinding.DialogNfcFiguresManagerBinding
import org.dolphinemu.dolphinemu.features.infinitybase.InfinityConfig
import org.dolphinemu.dolphinemu.features.infinitybase.model.Figure
import org.dolphinemu.dolphinemu.features.infinitybase.ui.FigureSlot
import org.dolphinemu.dolphinemu.features.infinitybase.ui.FigureSlotAdapter
import org.dolphinemu.dolphinemu.features.input.model.ControllerInterface
import org.dolphinemu.dolphinemu.features.input.model.DolphinSensorEventListener
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting
import org.dolphinemu.dolphinemu.features.settings.model.Settings
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivity
import org.dolphinemu.dolphinemu.features.skylanders.SkylanderConfig
import org.dolphinemu.dolphinemu.features.skylanders.model.Skylander
import org.dolphinemu.dolphinemu.features.skylanders.ui.SkylanderSlot
import org.dolphinemu.dolphinemu.features.skylanders.ui.SkylanderSlotAdapter
import org.dolphinemu.dolphinemu.fragments.EmulationFragment
import org.dolphinemu.dolphinemu.fragments.MenuFragment
import org.dolphinemu.dolphinemu.fragments.SaveLoadStateFragment
import org.dolphinemu.dolphinemu.fragments.SaveLoadStateFragment.SaveOrLoad
import org.dolphinemu.dolphinemu.overlay.InputOverlay
import org.dolphinemu.dolphinemu.overlay.InputOverlayPointer
import org.dolphinemu.dolphinemu.ui.main.MainPresenter
import org.dolphinemu.dolphinemu.ui.main.ThemeProvider
import org.dolphinemu.dolphinemu.utils.AfterDirectoryInitializationRunner
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper
import org.dolphinemu.dolphinemu.utils.ThemeHelper
import kotlin.math.roundToInt

class EmulationActivity : AppCompatActivity(), ThemeProvider {
    private var emulationFragment: EmulationFragment? = null

    private lateinit var settings: Settings

    override var themeId = 0

    private var menuVisible = false

    var isActivityRecreated = false
    private var paths: Array<String>? = null
    private var riivolution = false
    private var launchSystemMenu = false
    private var menuToastShown = false

    private var skylanderData = Skylander(-1, -1, "Slot")
    private var infinityFigureData = Figure(-1, "Position")
    private var skylanderSlot = -1
    private var infinityPosition = -1
    private var infinityListPosition = -1
    private lateinit var skylandersBinding: DialogNfcFiguresManagerBinding
    private lateinit var infinityBinding: DialogNfcFiguresManagerBinding

    private lateinit var binding: ActivityEmulationBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        ThemeHelper.setTheme(this)

        super.onCreate(savedInstanceState)

        MainPresenter.skipRescanningLibrary()

        if (savedInstanceState == null) {
            // Get params we were passed
            paths = intent.getStringArrayExtra(EXTRA_SELECTED_GAMES)
            riivolution = intent.getBooleanExtra(EXTRA_RIIVOLUTION, false)
            launchSystemMenu = intent.getBooleanExtra(EXTRA_SYSTEM_MENU, false)
            hasUserPausedEmulation =
                intent.getBooleanExtra(EXTRA_USER_PAUSED_EMULATION, false)
            menuToastShown = false
            isActivityRecreated = false
        } else {
            isActivityRecreated = true
            restoreState(savedInstanceState)
        }

        settings = Settings()
        settings.loadSettings()

        updateOrientation()

        // Set these options now so that the SurfaceView the game renders into is the right size.
        enableFullscreenImmersive()

        binding = ActivityEmulationBinding.inflate(layoutInflater)
        setContentView(binding.root)

        setInsets()

        // Find or create the EmulationFragment
        emulationFragment = supportFragmentManager
            .findFragmentById(R.id.frame_emulation_fragment) as EmulationFragment?
        if (emulationFragment == null) {
            emulationFragment = EmulationFragment.newInstance(paths, riivolution, launchSystemMenu)
            supportFragmentManager.beginTransaction()
                .add(R.id.frame_emulation_fragment, emulationFragment!!)
                .commit()
        }

        if (NativeLibrary.IsGameMetadataValid())
            title = NativeLibrary.GetCurrentTitleDescription()

        if (skylanderSlots.isEmpty()) {
            for (i in 0..7) {
                skylanderSlots.add(SkylanderSlot(getString(R.string.skylander_slot, i + 1), i))
            }
        }

        if (infinityFigures.isEmpty()) {
            infinityFigures.apply {
                add(FigureSlot(getString(R.string.infinity_hexagon_label), 0))
                add(FigureSlot(getString(R.string.infinity_p1_label), 1))
                add(FigureSlot(getString(R.string.infinity_p1a1_label), 3))
                add(FigureSlot(getString(R.string.infinity_p1a2_label), 5))
                add(FigureSlot(getString(R.string.infinity_p2_label), 2))
                add(FigureSlot(getString(R.string.infinity_p2a1_label), 4))
                add(FigureSlot(getString(R.string.infinity_p2a2_label), 6))
            }
        }
    }

    override fun onSaveInstanceState(outState: Bundle) {
        if (!isChangingConfigurations) {
            emulationFragment!!.saveTemporaryState()
        }

        outState.apply {
            putStringArray(EXTRA_SELECTED_GAMES, paths)
            putBoolean(EXTRA_USER_PAUSED_EMULATION, hasUserPausedEmulation)
            putBoolean(EXTRA_MENU_TOAST_SHOWN, menuToastShown)
            putInt(EXTRA_SKYLANDER_SLOT, skylanderSlot)
            putInt(EXTRA_SKYLANDER_ID, skylanderData.id)
            putInt(EXTRA_SKYLANDER_VAR, skylanderData.variant)
            putString(EXTRA_SKYLANDER_NAME, skylanderData.name)
            putInt(EXTRA_INFINITY_POSITION, infinityPosition)
            putInt(EXTRA_INFINITY_LIST_POSITION, infinityListPosition)
            putLong(EXTRA_INFINITY_NUM, infinityFigureData.number)
            putString(EXTRA_INFINITY_NAME, infinityFigureData.name)
        }
        super.onSaveInstanceState(outState)
    }

    fun restoreState(savedInstanceState: Bundle) {
        savedInstanceState.apply {
            paths = getStringArray(EXTRA_SELECTED_GAMES)
            hasUserPausedEmulation = getBoolean(EXTRA_USER_PAUSED_EMULATION)
            menuToastShown = savedInstanceState.getBoolean(EXTRA_MENU_TOAST_SHOWN)
            skylanderSlot = savedInstanceState.getInt(EXTRA_SKYLANDER_SLOT)
            skylanderData = Skylander(
                savedInstanceState.getInt(EXTRA_SKYLANDER_ID),
                savedInstanceState.getInt(EXTRA_SKYLANDER_VAR),
                savedInstanceState.getString(EXTRA_SKYLANDER_NAME)!!
            )
            infinityPosition = savedInstanceState.getInt(EXTRA_INFINITY_POSITION)
            infinityListPosition = savedInstanceState.getInt(EXTRA_INFINITY_LIST_POSITION)
            infinityFigureData = Figure(
                savedInstanceState.getLong(EXTRA_INFINITY_NUM),
                savedInstanceState.getString(EXTRA_INFINITY_NAME)!!
            )
        }
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        if (hasFocus) {
            enableFullscreenImmersive()
        }
    }

    override fun onResume() {
        ThemeHelper.setCorrectTheme(this)

        super.onResume()

        // Only android 9+ support this feature.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            val attributes = window.attributes

            attributes.layoutInDisplayCutoutMode =
                if (BooleanSetting.MAIN_EXPAND_TO_CUTOUT_AREA.boolean) {
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
                } else {
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER
                }

            window.attributes = attributes
        }

        updateOrientation()

        DolphinSensorEventListener.setDeviceRotation(windowManager.defaultDisplay.rotation)
    }

    override fun onStop() {
        super.onStop()
        settings.saveSettings(null)
    }

    fun onTitleChanged() {
        if (!menuToastShown) {
            // The reason why this doesn't run earlier is because we want to be sure the boot succeeded.
            Toast.makeText(this, R.string.emulation_menu_help, Toast.LENGTH_LONG).show()
            menuToastShown = true
        }

        try {
            title = NativeLibrary.GetCurrentTitleDescription()

            emulationFragment?.refreshInputOverlay()
        } catch (_: IllegalStateException) {
            // Most likely the core delivered an onTitleChanged while emulation was shutting down.
            // Let's just ignore it, since we're about to shut down anyway.
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        settings.close()
    }

    override fun onBackPressed() {
        if (!closeSubmenu()) {
            toggleMenu()
        }
    }

    override fun onKeyLongPress(keyCode: Int, event: KeyEvent): Boolean {
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            emulationFragment!!.stopEmulation()
            return true
        }
        return super.onKeyLongPress(keyCode, event)
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, result: Intent?) {
        super.onActivityResult(requestCode, resultCode, result)
        // If the user picked a file, as opposed to just backing out.
        if (resultCode == RESULT_OK) {
            if (requestCode == REQUEST_CHANGE_DISC) {
                NativeLibrary.ChangeDisc(result!!.data.toString())
            } else if (requestCode == REQUEST_SKYLANDER_FILE) {
                val slot = SkylanderConfig.loadSkylander(
                    skylanderSlots[skylanderSlot].portalSlot,
                    result!!.data.toString()
                )!!
                clearSkylander(skylanderSlot)
                skylanderSlots[skylanderSlot].portalSlot = slot.first!!
                skylanderSlots[skylanderSlot].label = slot.second!!
                skylandersBinding.figureManager.adapter!!.notifyItemChanged(skylanderSlot)
                skylanderSlot = -1
                skylanderData = Skylander.BLANK_SKYLANDER
            } else if (requestCode == REQUEST_CREATE_SKYLANDER) {
                if (skylanderData.id != -1 && skylanderData.variant != -1) {
                    val slot = SkylanderConfig.createSkylander(
                        skylanderData.id,
                        skylanderData.variant,
                        result!!.data.toString(),
                        skylanderSlots[skylanderSlot].portalSlot
                    )
                    clearSkylander(skylanderSlot)
                    skylanderSlots[skylanderSlot].portalSlot = slot.first
                    skylanderSlots[skylanderSlot].label = slot.second
                    skylandersBinding.figureManager.adapter?.notifyItemChanged(skylanderSlot)
                    skylanderSlot = -1
                    skylanderData = Skylander.BLANK_SKYLANDER
                }
            } else if (requestCode == REQUEST_INFINITY_FIGURE_FILE) {
                val label = InfinityConfig.loadFigure(infinityPosition, result!!.data.toString())
                if (label != null && label != "Unknown Figure") {
                    clearInfinityFigure(infinityListPosition)
                    infinityFigures[infinityListPosition].label = label
                    infinityBinding.figureManager.adapter?.notifyItemChanged(infinityListPosition)
                    infinityPosition = -1
                    infinityListPosition = -1
                    infinityFigureData = Figure.BLANK_FIGURE
                } else {
                    MaterialAlertDialogBuilder(this)
                        .setTitle(R.string.incompatible_figure_selected)
                        .setMessage(R.string.select_compatible_figure)
                        .setPositiveButton(android.R.string.ok, null)
                        .show()
                }
            } else if (requestCode == REQUEST_CREATE_INFINITY_FIGURE) {
                if (infinityFigureData.number != -1L) {
                    val label = InfinityConfig.createFigure(
                        infinityFigureData.number,
                        result!!.data.toString(),
                        infinityPosition
                    )
                    clearInfinityFigure(infinityListPosition)
                    infinityFigures[infinityListPosition].label = label!!
                    infinityBinding.figureManager.adapter?.notifyItemChanged(infinityListPosition)
                    infinityPosition = -1
                    infinityListPosition = -1
                    infinityFigureData = Figure.BLANK_FIGURE
                }
            }
        }
    }

    private fun enableFullscreenImmersive() {
        window.decorView.systemUiVisibility = View.SYSTEM_UI_FLAG_LAYOUT_STABLE or
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION or
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or
                View.SYSTEM_UI_FLAG_FULLSCREEN or
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
    }

    private fun updateOrientation() {
        requestedOrientation = IntSetting.MAIN_EMULATION_ORIENTATION.int
    }

    private fun closeSubmenu(): Boolean =
        supportFragmentManager.popBackStackImmediate(
            BACKSTACK_NAME_SUBMENU,
            FragmentManager.POP_BACK_STACK_INCLUSIVE
        )

    private fun closeMenu(): Boolean {
        menuVisible = false
        return supportFragmentManager.popBackStackImmediate(
            BACKSTACK_NAME_MENU,
            FragmentManager.POP_BACK_STACK_INCLUSIVE
        )
    }

    private fun toggleMenu() {
        if (!closeMenu()) {
            // Removing the menu failed, so that means it wasn't visible. Add it.
            val fragment: Fragment = MenuFragment.newInstance()
            supportFragmentManager.beginTransaction()
                .setCustomAnimations(
                    R.animator.menu_slide_in_from_start,
                    R.animator.menu_slide_out_to_start,
                    R.animator.menu_slide_in_from_start,
                    R.animator.menu_slide_out_to_start
                )
                .add(R.id.frame_menu, fragment)
                .addToBackStack(BACKSTACK_NAME_MENU)
                .commit()
            menuVisible = true
        }
    }

    private fun setInsets() =
        ViewCompat.setOnApplyWindowInsetsListener(binding.frameMenu) { v: View, windowInsets: WindowInsetsCompat ->
            val cutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())
            val mlpMenu = v.layoutParams as MarginLayoutParams
            val menuWidth = resources.getDimensionPixelSize(R.dimen.menu_width)
            if (ViewCompat.getLayoutDirection(v) == ViewCompat.LAYOUT_DIRECTION_LTR) {
                mlpMenu.width = cutInsets.left + menuWidth
            } else {
                mlpMenu.width = cutInsets.right + menuWidth
            }
            NativeLibrary.SetObscuredPixelsTop(cutInsets.top)
            NativeLibrary.SetObscuredPixelsLeft(cutInsets.left)
            windowInsets
        }

    fun showOverlayControlsMenu(anchor: View) {
        val popup = PopupMenu(this, anchor)
        val menu = popup.menu
        val wii = NativeLibrary.IsEmulatingWii()
        val id = if (wii) R.menu.menu_overlay_controls_wii else R.menu.menu_overlay_controls_gc
        popup.menuInflater.inflate(id, menu)

        // Populate the switch value for joystick center on touch
        menu.findItem(R.id.menu_emulation_joystick_rel_center).isChecked =
            BooleanSetting.MAIN_JOYSTICK_REL_CENTER.boolean
        if (wii) {
            menu.findItem(R.id.menu_emulation_ir_recenter).isChecked =
                BooleanSetting.MAIN_IR_ALWAYS_RECENTER.boolean
        }
        popup.setOnMenuItemClickListener { item: MenuItem -> onOptionsItemSelected(item) }
        popup.show()
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        val action = buttonsActionsMap[item.itemId, -1]
        if (action >= 0) {
            if (item.isCheckable) {
                // Need to pass a reference to the item to set the switch state, since it is not done automatically
                handleCheckableMenuAction(action, item)
            } else {
                handleMenuAction(action)
            }
        }
        return true
    }

    private fun handleCheckableMenuAction(menuAction: Int, item: MenuItem) {
        when (menuAction) {
            MENU_ACTION_JOYSTICK_REL_CENTER -> {
                item.isChecked = !item.isChecked
                toggleJoystickRelCenter(item.isChecked)
            }

            MENU_SET_IR_RECENTER -> {
                item.isChecked = !item.isChecked
                toggleRecenter(item.isChecked)
            }
        }
    }

    fun handleMenuAction(menuAction: Int) {
        when (menuAction) {
            MENU_ACTION_EDIT_CONTROLS_PLACEMENT -> editControlsPlacement()
            MENU_ACTION_RESET_OVERLAY -> resetOverlay()
            MENU_ACTION_TOGGLE_CONTROLS -> toggleControls()
            MENU_ACTION_LATCHING_CONTROLS -> latchingControls()
            MENU_ACTION_ADJUST_SCALE -> adjustScale()
            MENU_ACTION_CHOOSE_CONTROLLER -> chooseController()
            MENU_ACTION_REFRESH_WIIMOTES -> NativeLibrary.RefreshWiimotes()
            MENU_ACTION_PAUSE_EMULATION -> {
                hasUserPausedEmulation = true
                NativeLibrary.PauseEmulation()
            }

            MENU_ACTION_UNPAUSE_EMULATION -> {
                hasUserPausedEmulation = false
                NativeLibrary.UnPauseEmulation()
            }

            MENU_ACTION_TAKE_SCREENSHOT -> NativeLibrary.SaveScreenShot()
            MENU_ACTION_QUICK_SAVE -> NativeLibrary.SaveState(9, false)
            MENU_ACTION_QUICK_LOAD -> NativeLibrary.LoadState(9)
            MENU_ACTION_SAVE_ROOT -> showSubMenu(SaveOrLoad.SAVE)
            MENU_ACTION_LOAD_ROOT -> showSubMenu(SaveOrLoad.LOAD)
            MENU_ACTION_SAVE_SLOT1 -> NativeLibrary.SaveState(0, false)
            MENU_ACTION_SAVE_SLOT2 -> NativeLibrary.SaveState(1, false)
            MENU_ACTION_SAVE_SLOT3 -> NativeLibrary.SaveState(2, false)
            MENU_ACTION_SAVE_SLOT4 -> NativeLibrary.SaveState(3, false)
            MENU_ACTION_SAVE_SLOT5 -> NativeLibrary.SaveState(4, false)
            MENU_ACTION_SAVE_SLOT6 -> NativeLibrary.SaveState(5, false)
            MENU_ACTION_LOAD_SLOT1 -> NativeLibrary.LoadState(0)
            MENU_ACTION_LOAD_SLOT2 -> NativeLibrary.LoadState(1)
            MENU_ACTION_LOAD_SLOT3 -> NativeLibrary.LoadState(2)
            MENU_ACTION_LOAD_SLOT4 -> NativeLibrary.LoadState(3)
            MENU_ACTION_LOAD_SLOT5 -> NativeLibrary.LoadState(4)
            MENU_ACTION_LOAD_SLOT6 -> NativeLibrary.LoadState(5)
            MENU_ACTION_CHANGE_DISC -> {
                val intent = Intent(Intent.ACTION_OPEN_DOCUMENT)
                intent.addCategory(Intent.CATEGORY_OPENABLE)
                intent.type = "*/*"
                startActivityForResult(intent, REQUEST_CHANGE_DISC)
            }

            MENU_SET_IR_MODE -> setIRMode()
            MENU_ACTION_CHOOSE_DOUBLETAP -> chooseDoubleTapButton()
            MENU_ACTION_SETTINGS -> SettingsActivity.launch(this, MenuTag.SETTINGS)
            MENU_ACTION_SKYLANDERS -> showSkylanderPortalSettings()
            MENU_ACTION_INFINITY_BASE -> showInfinityBaseSettings()
            MENU_ACTION_EXIT -> emulationFragment!!.stopEmulation()
        }
    }

    private fun toggleJoystickRelCenter(state: Boolean) {
        BooleanSetting.MAIN_JOYSTICK_REL_CENTER.setBoolean(settings, state)
    }

    private fun toggleRecenter(state: Boolean) {
        BooleanSetting.MAIN_IR_ALWAYS_RECENTER.setBoolean(settings, state)
        emulationFragment?.refreshOverlayPointer()
    }

    private fun editControlsPlacement() {
        if (emulationFragment!!.isConfiguringControls) {
            emulationFragment?.stopConfiguringControls()
        } else {
            closeSubmenu()
            closeMenu()
            emulationFragment?.startConfiguringControls()
        }
    }

    // Gets button presses
    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (!menuVisible) {
            if (ControllerInterface.dispatchKeyEvent(event)) {
                return true
            }
        }
        return super.dispatchKeyEvent(event)
    }

    private fun latchingControls() {
        val builder = MaterialAlertDialogBuilder(this)
            .setTitle(R.string.emulation_latching_controls)

        when (InputOverlay.configuredControllerType) {
            InputOverlay.OVERLAY_GAMECUBE -> {
                val gcLatchingButtons = BooleanArray(8)
                val gcSettingBase = "MAIN_BUTTON_LATCHING_GC_"

                for (i in gcLatchingButtons.indices) {
                    gcLatchingButtons[i] = BooleanSetting.valueOf(gcSettingBase + i).boolean
                }

                builder.setMultiChoiceItems(
                    R.array.gcpadLatchableButtons, gcLatchingButtons
                ) { _: DialogInterface?, indexSelected: Int, isChecked: Boolean ->
                    BooleanSetting.valueOf(gcSettingBase + indexSelected)
                        .setBoolean(settings, isChecked)
                }
            }
            InputOverlay.OVERLAY_WIIMOTE_CLASSIC -> {
                val wiiClassicLatchingButtons = BooleanArray(11)
                val classicSettingBase = "MAIN_BUTTON_LATCHING_CLASSIC_"

                for (i in wiiClassicLatchingButtons.indices) {
                    wiiClassicLatchingButtons[i] = BooleanSetting.valueOf(classicSettingBase + i).boolean
                }
                builder.setMultiChoiceItems(
                    R.array.classicLatchableButtons, wiiClassicLatchingButtons
                ) { _: DialogInterface?, indexSelected: Int, isChecked: Boolean ->
                    BooleanSetting.valueOf(classicSettingBase + indexSelected)
                        .setBoolean(settings, isChecked)
                }
            }
            InputOverlay.OVERLAY_WIIMOTE_NUNCHUK -> {
                val nunchukLatchingButtons = BooleanArray(9)
                val nunchukSettingBase = "MAIN_BUTTON_LATCHING_WII_"

                // For OVERLAY_WIIMOTE_NUNCHUK, settings index 7 is the D-Pad (which cannot be
                // latching). C and Z (settings indices 8 and 9) need to map to multichoice array
                // indices 7 and 8 to avoid a gap.
                fun translateToSettingsIndex(idx: Int): Int = if (idx >= 7) idx + 1 else idx

                for (i in nunchukLatchingButtons.indices) {
                    nunchukLatchingButtons[i] = BooleanSetting
                        .valueOf(nunchukSettingBase + translateToSettingsIndex(i)).boolean
                }

                builder.setMultiChoiceItems(
                    R.array.nunchukLatchableButtons, nunchukLatchingButtons
                ) { _: DialogInterface?, indexSelected: Int, isChecked: Boolean ->
                    BooleanSetting.valueOf(nunchukSettingBase + translateToSettingsIndex(indexSelected))
                        .setBoolean(settings, isChecked)
                }
            }
            else -> {
                val wiimoteLatchingButtons = BooleanArray(7)
                val wiimoteSettingBase = "MAIN_BUTTON_LATCHING_WII_"

                for (i in wiimoteLatchingButtons.indices) {
                    wiimoteLatchingButtons[i] = BooleanSetting.valueOf(wiimoteSettingBase + i).boolean
                }

                builder.setMultiChoiceItems(
                      R.array.wiimoteLatchableButtons, wiimoteLatchingButtons
                ) { _: DialogInterface?, indexSelected: Int, isChecked: Boolean ->
                    BooleanSetting.valueOf(wiimoteSettingBase + indexSelected)
                        .setBoolean(settings, isChecked)
                }
            }
        }

        builder
            .setPositiveButton(R.string.ok) { _: DialogInterface?, _: Int ->
                emulationFragment?.refreshInputOverlay()
            }
            .show()
    }

    private fun toggleControls() {
        val builder = MaterialAlertDialogBuilder(this)
            .setTitle(R.string.emulation_toggle_controls)

        val currentController = InputOverlay.configuredControllerType

        if (currentController == InputOverlay.OVERLAY_GAMECUBE) {
            val gcEnabledButtons = BooleanArray(11)
            val gcSettingBase = "MAIN_BUTTON_TOGGLE_GC_"

            for (i in gcEnabledButtons.indices) {
                gcEnabledButtons[i] = BooleanSetting.valueOf(gcSettingBase + i).boolean
            }
            builder.setMultiChoiceItems(
                R.array.gcpadButtons, gcEnabledButtons
            ) { _: DialogInterface?, indexSelected: Int, isChecked: Boolean ->
                BooleanSetting
                    .valueOf(gcSettingBase + indexSelected).setBoolean(settings, isChecked)
            }
        } else if (currentController == InputOverlay.OVERLAY_WIIMOTE_CLASSIC) {
            val wiiClassicEnabledButtons = BooleanArray(14)
            val classicSettingBase = "MAIN_BUTTON_TOGGLE_CLASSIC_"

            for (i in wiiClassicEnabledButtons.indices) {
                wiiClassicEnabledButtons[i] = BooleanSetting.valueOf(classicSettingBase + i).boolean
            }
            builder.setMultiChoiceItems(
                R.array.classicButtons, wiiClassicEnabledButtons
            ) { _: DialogInterface?, indexSelected: Int, isChecked: Boolean ->
                BooleanSetting.valueOf(classicSettingBase + indexSelected)
                    .setBoolean(settings, isChecked)
            }
        } else {
            val wiiEnabledButtons = BooleanArray(11)
            val wiiSettingBase = "MAIN_BUTTON_TOGGLE_WII_"

            for (i in wiiEnabledButtons.indices) {
                wiiEnabledButtons[i] = BooleanSetting.valueOf(wiiSettingBase + i).boolean
            }
            if (currentController == InputOverlay.OVERLAY_WIIMOTE_NUNCHUK) {
                builder.setMultiChoiceItems(
                    R.array.nunchukButtons, wiiEnabledButtons
                ) { _: DialogInterface?, indexSelected: Int, isChecked: Boolean ->
                    BooleanSetting
                        .valueOf(wiiSettingBase + indexSelected).setBoolean(settings, isChecked)
                }
            } else {
                builder.setMultiChoiceItems(
                    R.array.wiimoteButtons, wiiEnabledButtons
                ) { _: DialogInterface?, indexSelected: Int, isChecked: Boolean ->
                    BooleanSetting
                        .valueOf(wiiSettingBase + indexSelected).setBoolean(settings, isChecked)
                }
            }
        }

        builder
            .setNeutralButton(R.string.emulation_toggle_all) { _: DialogInterface?, _: Int ->
                emulationFragment!!.toggleInputOverlayVisibility(settings)
            }
            .setPositiveButton(R.string.ok) { _: DialogInterface?, _: Int -> emulationFragment?.refreshInputOverlay() }
            .show()
    }

    private fun chooseDoubleTapButton() {
        val currentValue = IntSetting.MAIN_DOUBLE_TAP_BUTTON.int

        val buttonList =
            if (InputOverlay.configuredControllerType == InputOverlay.OVERLAY_WIIMOTE_CLASSIC) R.array.doubleTapWithClassic else R.array.doubleTap

        var checkedItem = -1
        val itemCount = resources.getStringArray(buttonList).size
        for (i in 0 until itemCount) {
            if (InputOverlayPointer.DOUBLE_TAP_OPTIONS[i] == currentValue) {
                checkedItem = i
                break
            }
        }

        MaterialAlertDialogBuilder(this)
            .setSingleChoiceItems(buttonList, checkedItem) { _: DialogInterface?, which: Int ->
                IntSetting.MAIN_DOUBLE_TAP_BUTTON.setInt(
                    settings,
                    InputOverlayPointer.DOUBLE_TAP_OPTIONS[which]
                )
            }
            .setPositiveButton(R.string.ok) { _: DialogInterface?, _: Int ->
                emulationFragment?.initInputPointer()
            }
            .show()
    }

    private fun adjustScale() {
        val dialogBinding = DialogInputAdjustBinding.inflate(layoutInflater)
        dialogBinding.apply {
            inputScaleSlider.apply {
                valueTo = 150f
                value = IntSetting.MAIN_CONTROL_SCALE.int.toFloat()
                stepSize = 1f
                addOnChangeListener(Slider.OnChangeListener { _: Slider?, progress: Float, _: Boolean ->
                    dialogBinding.inputScaleValue.text = "${(progress.toInt() + 50)}%"
                })
            }
            inputScaleValue.text =
                "${(dialogBinding.inputScaleSlider.value.toInt() + 50)}%"

            inputOpacitySlider.apply {
                valueTo = 100f
                value = IntSetting.MAIN_CONTROL_OPACITY.int.toFloat()
                stepSize = 1f
                addOnChangeListener(Slider.OnChangeListener { _: Slider?, progress: Float, _: Boolean ->
                    inputOpacityValue.text = progress.toInt().toString() + "%"
                })
            }
            inputOpacityValue.text = inputOpacitySlider.value.toInt().toString() + "%"
        }

        MaterialAlertDialogBuilder(this)
            .setTitle(R.string.emulation_control_adjustments)
            .setView(dialogBinding.root)
            .setPositiveButton(R.string.ok) { _: DialogInterface?, _: Int ->
                IntSetting.MAIN_CONTROL_SCALE.setInt(
                    settings,
                    dialogBinding.inputScaleSlider.value.toInt()
                )
                IntSetting.MAIN_CONTROL_OPACITY.setInt(
                    settings,
                    dialogBinding.inputOpacitySlider.value.toInt()
                )
                emulationFragment?.refreshInputOverlay()
            }
            .setNeutralButton(R.string.default_values) { _: DialogInterface?, _: Int ->
                IntSetting.MAIN_CONTROL_SCALE.delete(settings)
                IntSetting.MAIN_CONTROL_OPACITY.delete(settings)
                emulationFragment?.refreshInputOverlay()
            }
            .show()
    }

    private fun addControllerIfNotNone(
        entries: MutableList<CharSequence>,
        values: MutableList<Int>,
        controller: IntSetting,
        entry: Int,
        value: Int
    ) {
        if (controller.int != 0) {
            entries.add(getString(entry))
            values.add(value)
        }
    }

    private fun chooseController() {
        val entries = ArrayList<CharSequence>()
        val values = ArrayList<Int>()

        entries.add(getString(R.string.none))
        values.add(-1)

        addControllerIfNotNone(
            entries,
            values,
            IntSetting.MAIN_SI_DEVICE_0,
            R.string.controller_0,
            0
        )
        addControllerIfNotNone(
            entries,
            values,
            IntSetting.MAIN_SI_DEVICE_1,
            R.string.controller_1,
            1
        )
        addControllerIfNotNone(
            entries,
            values,
            IntSetting.MAIN_SI_DEVICE_2,
            R.string.controller_2,
            2
        )
        addControllerIfNotNone(
            entries,
            values,
            IntSetting.MAIN_SI_DEVICE_3,
            R.string.controller_3,
            3
        )

        if (NativeLibrary.IsEmulatingWii()) {
            addControllerIfNotNone(
                entries,
                values,
                IntSetting.WIIMOTE_1_SOURCE,
                R.string.wiimote_0,
                4
            )
            addControllerIfNotNone(
                entries,
                values,
                IntSetting.WIIMOTE_2_SOURCE,
                R.string.wiimote_1,
                5
            )
            addControllerIfNotNone(
                entries,
                values,
                IntSetting.WIIMOTE_3_SOURCE,
                R.string.wiimote_2,
                6
            )
            addControllerIfNotNone(
                entries,
                values,
                IntSetting.WIIMOTE_4_SOURCE,
                R.string.wiimote_3,
                7
            )
        }

        val controllerSetting =
            if (NativeLibrary.IsEmulatingWii()) IntSetting.MAIN_OVERLAY_WII_CONTROLLER else IntSetting.MAIN_OVERLAY_GC_CONTROLLER
        val currentValue = controllerSetting.int
        var checkedItem = -1
        for (i in values.indices) {
            if (values[i] == currentValue) {
                checkedItem = i
                break
            }
        }

        MaterialAlertDialogBuilder(this)
            .setTitle(R.string.emulation_choose_controller)
            .setSingleChoiceItems(
                entries.toArray(arrayOf<CharSequence>()), checkedItem
            ) { _: DialogInterface?, indexSelected: Int ->
                controllerSetting.setInt(settings, values[indexSelected])
            }
            .setPositiveButton(R.string.ok) { _: DialogInterface?, _: Int ->
                emulationFragment?.refreshInputOverlay()
            }
            .setNeutralButton(
                R.string.emulation_more_controller_settings
            ) { _: DialogInterface?, _: Int -> SettingsActivity.launch(this, MenuTag.SETTINGS) }
            .show()
    }

    private fun setIRMode() {
        MaterialAlertDialogBuilder(this)
            .setTitle(R.string.emulation_ir_mode)
            .setSingleChoiceItems(
                R.array.irModeEntries,
                IntSetting.MAIN_IR_MODE.int
            ) { _: DialogInterface?, indexSelected: Int ->
                IntSetting.MAIN_IR_MODE.setInt(settings, indexSelected)
            }
            .setPositiveButton(R.string.ok) { _: DialogInterface?, _: Int ->
                emulationFragment?.refreshOverlayPointer()
            }
            .show()
    }

    private fun showSkylanderPortalSettings() {
        skylandersBinding = DialogNfcFiguresManagerBinding.inflate(layoutInflater)
        skylandersBinding.figureManager.apply {
            layoutManager = LinearLayoutManager(this@EmulationActivity)
            adapter = SkylanderSlotAdapter(skylanderSlots, this@EmulationActivity)
        }

        MaterialAlertDialogBuilder(this)
            .setTitle(R.string.skylanders_manager)
            .setView(skylandersBinding.root)
            .show()
    }

    private fun showInfinityBaseSettings() {
        infinityBinding = DialogNfcFiguresManagerBinding.inflate(layoutInflater)
        infinityBinding.figureManager.apply {
            layoutManager = LinearLayoutManager(this@EmulationActivity)
            adapter = FigureSlotAdapter(infinityFigures, this@EmulationActivity)
        }

        MaterialAlertDialogBuilder(this)
            .setTitle(R.string.infinity_manager)
            .setView(infinityBinding.root)
            .show()
    }

    fun setSkylanderData(id: Int, variant: Int, name: String, slot: Int) {
        skylanderData = Skylander(id, variant, name)
        skylanderSlot = slot
    }

    fun clearSkylander(slot: Int) {
        skylanderSlots[slot].label = getString(R.string.skylander_slot, slot + 1)
        skylandersBinding.figureManager.adapter?.notifyItemChanged(slot)
    }

    fun setInfinityFigureData(num: Long, name: String, position: Int, listPosition: Int) {
        infinityFigureData = Figure(num, name)
        infinityPosition = position
        infinityListPosition = listPosition
    }

    fun clearInfinityFigure(position: Int) {
        when (position) {
            0 -> infinityFigures[position].label = getString(R.string.infinity_hexagon_label)
            1 -> infinityFigures[position].label = getString(R.string.infinity_p1_label)
            2 -> infinityFigures[position].label = getString(R.string.infinity_p1a1_label)
            3 -> infinityFigures[position].label = getString(R.string.infinity_p1a2_label)
            4 -> infinityFigures[position].label = getString(R.string.infinity_p2_label)
            5 -> infinityFigures[position].label = getString(R.string.infinity_p2a1_label)
            6 -> infinityFigures[position].label = getString(R.string.infinity_p2a2_label)
        }
        infinityBinding.figureManager.adapter?.notifyItemChanged(position)
    }

    private fun resetOverlay() {
        MaterialAlertDialogBuilder(this)
            .setTitle(getString(R.string.emulation_touch_overlay_reset))
            .setPositiveButton(R.string.yes) { _: DialogInterface?, _: Int ->
                emulationFragment?.resetInputOverlay()
            }
            .setNegativeButton(R.string.cancel, null)
            .show()
    }

    override fun dispatchTouchEvent(event: MotionEvent): Boolean {
        if (event.actionMasked == MotionEvent.ACTION_DOWN) {
            var anyMenuClosed = false

            var submenu = supportFragmentManager.findFragmentById(R.id.frame_submenu)
            if (submenu != null && areCoordinatesOutside(submenu.view, event.x, event.y)) {
                closeSubmenu()
                submenu = null
                anyMenuClosed = true
            }

            if (submenu == null) {
                val menu = supportFragmentManager.findFragmentById(R.id.frame_menu)
                if (menu != null && areCoordinatesOutside(menu.view, event.x, event.y)) {
                    closeMenu()
                    anyMenuClosed = true
                }
            }

            if (anyMenuClosed)
                return true
        }
        return super.dispatchTouchEvent(event)
    }

    override fun dispatchGenericMotionEvent(event: MotionEvent): Boolean {
        if (!menuVisible) {
            if (ControllerInterface.dispatchGenericMotionEvent(event)) {
                return true
            }
        }
        return super.dispatchGenericMotionEvent(event)
    }

    private fun showSubMenu(saveOrLoad: SaveOrLoad) {
        // Get rid of any visible submenu
        supportFragmentManager.popBackStack(
            BACKSTACK_NAME_SUBMENU,
            FragmentManager.POP_BACK_STACK_INCLUSIVE
        )

        val fragment: Fragment = SaveLoadStateFragment.newInstance(saveOrLoad)
        supportFragmentManager.beginTransaction()
            .setCustomAnimations(
                R.animator.menu_slide_in_from_end,
                R.animator.menu_slide_out_to_end,
                R.animator.menu_slide_in_from_end,
                R.animator.menu_slide_out_to_end
            )
            .replace(R.id.frame_submenu, fragment)
            .addToBackStack(BACKSTACK_NAME_SUBMENU)
            .commit()
    }

    fun initInputPointer() {
        emulationFragment?.initInputPointer()
    }

    override fun setTheme(themeId: Int) {
        super.setTheme(themeId)
        this.themeId = themeId
    }

    companion object {
        private const val BACKSTACK_NAME_MENU = "menu"
        private const val BACKSTACK_NAME_SUBMENU = "submenu"
        const val REQUEST_CHANGE_DISC = 1
        const val REQUEST_SKYLANDER_FILE = 2
        const val REQUEST_CREATE_SKYLANDER = 3
        const val REQUEST_INFINITY_FIGURE_FILE = 4
        const val REQUEST_CREATE_INFINITY_FIGURE = 5

        private var ignoreLaunchRequests = false

        var hasUserPausedEmulation = false

        private val skylanderSlots: MutableList<SkylanderSlot> = ArrayList()
        private val infinityFigures: MutableList<FigureSlot> = ArrayList()
        private val buttonsActionsMap = SparseIntArray()

        const val EXTRA_SELECTED_GAMES = "SelectedGames"
        const val EXTRA_RIIVOLUTION = "Riivolution"
        const val EXTRA_SYSTEM_MENU = "SystemMenu"
        const val EXTRA_USER_PAUSED_EMULATION = "sUserPausedEmulation"
        const val EXTRA_MENU_TOAST_SHOWN = "MenuToastShown"
        const val EXTRA_SKYLANDER_SLOT = "SkylanderSlot"
        const val EXTRA_SKYLANDER_ID = "SkylanderId"
        const val EXTRA_SKYLANDER_VAR = "SkylanderVar"
        const val EXTRA_SKYLANDER_NAME = "SkylanderName"
        const val EXTRA_INFINITY_POSITION = "FigurePosition"
        const val EXTRA_INFINITY_LIST_POSITION = "FigureListPosition"
        const val EXTRA_INFINITY_NUM = "FigureNum"
        const val EXTRA_INFINITY_NAME = "FigureName"
        const val MENU_ACTION_EDIT_CONTROLS_PLACEMENT = 0
        const val MENU_ACTION_TOGGLE_CONTROLS = 1
        const val MENU_ACTION_ADJUST_SCALE = 2
        const val MENU_ACTION_CHOOSE_CONTROLLER = 3
        const val MENU_ACTION_REFRESH_WIIMOTES = 4
        const val MENU_ACTION_TAKE_SCREENSHOT = 5
        const val MENU_ACTION_QUICK_SAVE = 6
        const val MENU_ACTION_QUICK_LOAD = 7
        const val MENU_ACTION_SAVE_ROOT = 8
        const val MENU_ACTION_LOAD_ROOT = 9
        const val MENU_ACTION_SAVE_SLOT1 = 10
        const val MENU_ACTION_SAVE_SLOT2 = 11
        const val MENU_ACTION_SAVE_SLOT3 = 12
        const val MENU_ACTION_SAVE_SLOT4 = 13
        const val MENU_ACTION_SAVE_SLOT5 = 14
        const val MENU_ACTION_SAVE_SLOT6 = 15
        const val MENU_ACTION_LOAD_SLOT1 = 16
        const val MENU_ACTION_LOAD_SLOT2 = 17
        const val MENU_ACTION_LOAD_SLOT3 = 18
        const val MENU_ACTION_LOAD_SLOT4 = 19
        const val MENU_ACTION_LOAD_SLOT5 = 20
        const val MENU_ACTION_LOAD_SLOT6 = 21
        const val MENU_ACTION_EXIT = 22
        const val MENU_ACTION_CHANGE_DISC = 23
        const val MENU_ACTION_JOYSTICK_REL_CENTER = 24
        const val MENU_ACTION_RESET_OVERLAY = 26
        const val MENU_SET_IR_RECENTER = 27
        const val MENU_SET_IR_MODE = 28
        const val MENU_ACTION_CHOOSE_DOUBLETAP = 30
        const val MENU_ACTION_PAUSE_EMULATION = 32
        const val MENU_ACTION_UNPAUSE_EMULATION = 33
        const val MENU_ACTION_OVERLAY_CONTROLS = 34
        const val MENU_ACTION_SETTINGS = 35
        const val MENU_ACTION_SKYLANDERS = 36
        const val MENU_ACTION_INFINITY_BASE = 37
        const val MENU_ACTION_LATCHING_CONTROLS = 38

        init {
            buttonsActionsMap.apply {
                append(R.id.menu_emulation_edit_layout, MENU_ACTION_EDIT_CONTROLS_PLACEMENT)
                append(R.id.menu_emulation_toggle_controls, MENU_ACTION_TOGGLE_CONTROLS)
                append(R.id.menu_emulation_latching_controls, MENU_ACTION_LATCHING_CONTROLS)
                append(R.id.menu_emulation_adjust_scale, MENU_ACTION_ADJUST_SCALE)
                append(R.id.menu_emulation_choose_controller, MENU_ACTION_CHOOSE_CONTROLLER)
                append(R.id.menu_emulation_joystick_rel_center, MENU_ACTION_JOYSTICK_REL_CENTER)
                append(R.id.menu_emulation_reset_overlay, MENU_ACTION_RESET_OVERLAY)
                append(R.id.menu_emulation_ir_recenter, MENU_SET_IR_RECENTER)
                append(R.id.menu_emulation_set_ir_mode, MENU_SET_IR_MODE)
                append(R.id.menu_emulation_choose_doubletap, MENU_ACTION_CHOOSE_DOUBLETAP)
            }
        }

        @JvmStatic
        fun launch(activity: FragmentActivity, filePaths: Array<String>, riivolution: Boolean, fromIntent: Boolean = false) {
            if (ignoreLaunchRequests)
                return

            performLaunchChecks(activity, fromIntent) { launchWithoutChecks(activity, filePaths, riivolution) }
        }

        @JvmStatic
        fun launch(activity: FragmentActivity, filePath: String, riivolution: Boolean, fromIntent: Boolean = false) =
            launch(activity, arrayOf(filePath), riivolution, fromIntent)

        private fun launchWithoutChecks(
            activity: FragmentActivity,
            filePaths: Array<String>,
            riivolution: Boolean
        ) {
            ignoreLaunchRequests = true
            val launcher = Intent(activity, EmulationActivity::class.java)
            launcher.putExtra(EXTRA_SELECTED_GAMES, filePaths)
            launcher.putExtra(EXTRA_RIIVOLUTION, riivolution)
            activity.startActivity(launcher)
        }

        private fun performLaunchChecks(activity: FragmentActivity, fromIntent: Boolean, continueCallback: Runnable) {
            AfterDirectoryInitializationRunner().runWithLifecycle(activity) {
                if (fromIntent) {
                    activity.finish()
                }
                if (!FileBrowserHelper.isPathEmptyOrValid(StringSetting.MAIN_DEFAULT_ISO) ||
                    !FileBrowserHelper.isPathEmptyOrValid(StringSetting.MAIN_FS_PATH) ||
                    !FileBrowserHelper.isPathEmptyOrValid(StringSetting.MAIN_DUMP_PATH) ||
                    !FileBrowserHelper.isPathEmptyOrValid(StringSetting.MAIN_LOAD_PATH) ||
                    !FileBrowserHelper.isPathEmptyOrValid(StringSetting.MAIN_RESOURCEPACK_PATH)
                ) {
                    MaterialAlertDialogBuilder(activity)
                        .setMessage(R.string.unavailable_paths)
                        .setPositiveButton(R.string.yes) { _: DialogInterface?, _: Int ->
                            SettingsActivity.launch(activity, MenuTag.CONFIG_PATHS)
                        }
                        .setNeutralButton(R.string.continue_anyway) { _: DialogInterface?, _: Int ->
                            continueCallback.run()
                        }
                        .show()
                } else if (!FileBrowserHelper.isPathEmptyOrValid(StringSetting.MAIN_WII_SD_CARD_IMAGE_PATH) ||
                    !FileBrowserHelper.isPathEmptyOrValid(StringSetting.MAIN_WII_SD_CARD_SYNC_FOLDER_PATH)
                ) {
                    MaterialAlertDialogBuilder(activity)
                        .setMessage(R.string.unavailable_paths)
                        .setPositiveButton(R.string.yes) { _: DialogInterface?, _: Int ->
                            SettingsActivity.launch(activity, MenuTag.CONFIG_WII)
                        }
                        .setNeutralButton(R.string.continue_anyway) { _: DialogInterface?, _: Int ->
                            continueCallback.run()
                        }
                        .show()
                } else {
                    continueCallback.run()
                }
            }
        }

        @JvmStatic
        fun launchSystemMenu(activity: FragmentActivity) {
            if (ignoreLaunchRequests)
                return

            performLaunchChecks(activity, false) { launchSystemMenuWithoutChecks(activity) }
        }

        private fun launchSystemMenuWithoutChecks(activity: FragmentActivity) {
            ignoreLaunchRequests = true
            val launcher = Intent(activity, EmulationActivity::class.java)
            launcher.putExtra(EXTRA_SYSTEM_MENU, true)
            activity.startActivity(launcher)
        }

        @JvmStatic
        fun stopIgnoringLaunchRequests() {
            ignoreLaunchRequests = false
        }

        private fun areCoordinatesOutside(view: View?, x: Float, y: Float): Boolean {
            if (view == null)
                return true

            val viewBounds = Rect()
            view.getGlobalVisibleRect(viewBounds)
            return !viewBounds.contains(x.roundToInt(), y.roundToInt())
        }
    }
}
