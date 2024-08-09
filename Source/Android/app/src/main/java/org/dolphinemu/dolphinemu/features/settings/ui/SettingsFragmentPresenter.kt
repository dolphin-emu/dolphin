// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui

import android.content.Context
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.text.TextUtils
import androidx.appcompat.app.AppCompatActivity
import androidx.collection.ArraySet
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.dolphinemu.dolphinemu.NativeLibrary
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.activities.UserDataActivity
import org.dolphinemu.dolphinemu.features.input.model.ControlGroupEnabledSetting
import org.dolphinemu.dolphinemu.features.input.model.InputMappingBooleanSetting
import org.dolphinemu.dolphinemu.features.input.model.InputMappingDoubleSetting
import org.dolphinemu.dolphinemu.features.input.model.InputMappingIntSetting
import org.dolphinemu.dolphinemu.features.input.model.controlleremu.ControlGroup
import org.dolphinemu.dolphinemu.features.input.model.controlleremu.EmulatedController
import org.dolphinemu.dolphinemu.features.input.model.controlleremu.NumericSetting
import org.dolphinemu.dolphinemu.features.input.model.view.InputDeviceSetting
import org.dolphinemu.dolphinemu.features.input.model.view.InputMappingControlSetting
import org.dolphinemu.dolphinemu.features.input.ui.ProfileDialog
import org.dolphinemu.dolphinemu.features.input.ui.ProfileDialogPresenter
import org.dolphinemu.dolphinemu.features.settings.model.*
import org.dolphinemu.dolphinemu.features.settings.model.view.*
import org.dolphinemu.dolphinemu.model.GpuDriverMetadata
import org.dolphinemu.dolphinemu.ui.main.MainPresenter
import org.dolphinemu.dolphinemu.utils.*
import java.util.*
import kotlin.collections.ArrayList
import kotlin.math.ceil
import kotlin.math.floor

class SettingsFragmentPresenter(
    private val fragmentView: SettingsFragmentView,
    private val context: Context
) {
    private lateinit var menuTag: MenuTag
    private var gameId: String? = null

    private var settingsList: ArrayList<SettingsItem>? = null
    private var hasOldControllerSettings = false

    private var serialPort1Type = 0
    private var controllerNumber = 0
    private var controllerType = 0

    var gpuDriver: GpuDriverMetadata? = null
    private val libNameSetting: StringSetting = StringSetting.GFX_DRIVER_LIB_NAME

    fun onCreate(menuTag: MenuTag, gameId: String?, extras: Bundle) {
        this.gameId = gameId
        this.menuTag = menuTag

        if (menuTag.isGCPadMenu || menuTag.isWiimoteExtensionMenu) {
            controllerNumber = menuTag.subType
            controllerType = extras.getInt(ARG_CONTROLLER_TYPE)
        } else if (menuTag.isWiimoteMenu || menuTag.isWiimoteSubmenu) {
            controllerNumber = menuTag.subType
        } else if (menuTag.isSerialPort1Menu) {
            serialPort1Type = extras.getInt(ARG_SERIALPORT1_TYPE)
        } else if (
            menuTag == MenuTag.GRAPHICS
            && this.gameId.isNullOrEmpty()
            && !NativeLibrary.IsRunning()
            && GpuDriverHelper.supportsCustomDriverLoading()
        ) {
            this.gpuDriver =
                GpuDriverHelper.getInstalledDriverMetadata()
                    ?: GpuDriverHelper.getSystemDriverMetadata(context.applicationContext)
        }
    }

    fun onViewCreated(menuTag: MenuTag, settings: Settings?) {
        this.menuTag = menuTag

        if (!TextUtils.isEmpty(gameId)) {
            fragmentView.fragmentActivity.title = context.getString(R.string.game_settings, gameId)
        }

        this.settings = settings
    }

    var settings: Settings? = null
        set(settings) {
            field = settings
            if (settingsList == null && settings != null) {
                loadSettingsList()
            } else {
                fragmentView.showSettingsList(settingsList!!)
                fragmentView.setOldControllerSettingsWarningVisibility(hasOldControllerSettings)
            }
        }

    fun loadDefaultSettings() {
        loadSettingsList()
    }

    private fun loadSettingsList() {
        val sl = ArrayList<SettingsItem>()
        when (menuTag) {
            MenuTag.SETTINGS -> addTopLevelSettings(sl)
            MenuTag.CONFIG -> addConfigSettings(sl)
            MenuTag.CONFIG_GENERAL -> addGeneralSettings(sl)
            MenuTag.CONFIG_INTERFACE -> addInterfaceSettings(sl)
            MenuTag.CONFIG_AUDIO -> addAudioSettings(sl)
            MenuTag.CONFIG_PATHS -> addPathsSettings(sl)
            MenuTag.CONFIG_GAME_CUBE -> addGameCubeSettings(sl)
            MenuTag.CONFIG_WII -> addWiiSettings(sl)
            MenuTag.CONFIG_ADVANCED -> addAdvancedSettings(sl)
            MenuTag.GRAPHICS -> addGraphicsSettings(sl)
            MenuTag.CONFIG_SERIALPORT1 -> addSerialPortSubSettings(sl, serialPort1Type)
            MenuTag.GCPAD_TYPE -> addGcPadSettings(sl)
            MenuTag.WIIMOTE -> addWiimoteSettings(sl)
            MenuTag.ENHANCEMENTS -> addEnhanceSettings(sl)
            MenuTag.COLOR_CORRECTION -> addColorCorrectionSettings(sl)
            MenuTag.STEREOSCOPY -> addStereoSettings(sl)
            MenuTag.HACKS -> addHackSettings(sl)
            MenuTag.STATISTICS -> addStatisticsSettings(sl)
            MenuTag.ADVANCED_GRAPHICS -> addAdvancedGraphicsSettings(sl)
            MenuTag.CONFIG_LOG -> addLogConfigurationSettings(sl)
            MenuTag.DEBUG -> addDebugSettings(sl)
            MenuTag.GCPAD_1,
            MenuTag.GCPAD_2,
            MenuTag.GCPAD_3,
            MenuTag.GCPAD_4 -> addGcPadSubSettings(
                sl,
                controllerNumber,
                controllerType
            )

            MenuTag.WIIMOTE_1,
            MenuTag.WIIMOTE_2,
            MenuTag.WIIMOTE_3,
            MenuTag.WIIMOTE_4 -> addWiimoteSubSettings(
                sl,
                controllerNumber
            )

            MenuTag.WIIMOTE_EXTENSION_1,
            MenuTag.WIIMOTE_EXTENSION_2,
            MenuTag.WIIMOTE_EXTENSION_3,
            MenuTag.WIIMOTE_EXTENSION_4 -> addExtensionTypeSettings(
                sl,
                controllerNumber,
                controllerType
            )

            MenuTag.WIIMOTE_GENERAL_1,
            MenuTag.WIIMOTE_GENERAL_2,
            MenuTag.WIIMOTE_GENERAL_3,
            MenuTag.WIIMOTE_GENERAL_4 -> addWiimoteGeneralSubSettings(
                sl,
                controllerNumber
            )

            MenuTag.WIIMOTE_MOTION_SIMULATION_1,
            MenuTag.WIIMOTE_MOTION_SIMULATION_2,
            MenuTag.WIIMOTE_MOTION_SIMULATION_3,
            MenuTag.WIIMOTE_MOTION_SIMULATION_4 -> addWiimoteMotionSimulationSubSettings(
                sl,
                controllerNumber
            )

            MenuTag.WIIMOTE_MOTION_INPUT_1,
            MenuTag.WIIMOTE_MOTION_INPUT_2,
            MenuTag.WIIMOTE_MOTION_INPUT_3,
            MenuTag.WIIMOTE_MOTION_INPUT_4 -> addWiimoteMotionInputSubSettings(
                sl,
                controllerNumber
            )

            else -> throw UnsupportedOperationException("Unimplemented menu")
        }

        settingsList = sl
        fragmentView.showSettingsList(settingsList!!)
    }

    private fun addTopLevelSettings(sl: ArrayList<SettingsItem>) {
        sl.add(SubmenuSetting(context, R.string.config, MenuTag.CONFIG))
        sl.add(SubmenuSetting(context, R.string.graphics_settings, MenuTag.GRAPHICS))

        sl.add(SubmenuSetting(context, R.string.gcpad_settings, MenuTag.GCPAD_TYPE))
        if (settings!!.isWii) {
            sl.add(SubmenuSetting(context, R.string.wiimote_settings, MenuTag.WIIMOTE))
        }

        sl.add(HeaderSetting(context, R.string.setting_clear_info, 0))
    }

    private fun addConfigSettings(sl: ArrayList<SettingsItem>) {
        sl.add(SubmenuSetting(context, R.string.general_submenu, MenuTag.CONFIG_GENERAL))
        sl.add(SubmenuSetting(context, R.string.interface_submenu, MenuTag.CONFIG_INTERFACE))
        sl.add(SubmenuSetting(context, R.string.audio_submenu, MenuTag.CONFIG_AUDIO))
        sl.add(SubmenuSetting(context, R.string.paths_submenu, MenuTag.CONFIG_PATHS))
        sl.add(SubmenuSetting(context, R.string.gamecube_submenu, MenuTag.CONFIG_GAME_CUBE))
        sl.add(SubmenuSetting(context, R.string.wii_submenu, MenuTag.CONFIG_WII))
        sl.add(SubmenuSetting(context, R.string.advanced_submenu, MenuTag.CONFIG_ADVANCED))
        sl.add(SubmenuSetting(context, R.string.log_submenu, MenuTag.CONFIG_LOG))
        sl.add(SubmenuSetting(context, R.string.debug_submenu, MenuTag.DEBUG))
        sl.add(
            RunRunnable(context, R.string.user_data_submenu, 0, 0, 0, false)
            { UserDataActivity.launch(context) }
        )
    }

    private fun addGeneralSettings(sl: ArrayList<SettingsItem>) {
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_CPU_THREAD,
                R.string.dual_core,
                R.string.dual_core_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_ENABLE_CHEATS,
                R.string.enable_cheats,
                0
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_OVERRIDE_REGION_SETTINGS,
                R.string.override_region_settings,
                0
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_AUTO_DISC_CHANGE,
                R.string.auto_disc_change,
                0
            )
        )
        sl.add(
            PercentSliderSetting(
                context,
                FloatSetting.MAIN_EMULATION_SPEED,
                R.string.speed_limit,
                0,
                0f,
                200f,
                "%",
                1f,
                false
            )
        )
        sl.add(
            SingleChoiceSetting(
                context,
                IntSetting.MAIN_FALLBACK_REGION,
                R.string.fallback_region,
                0,
                R.array.regionEntries,
                R.array.regionValues
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_ANALYTICS_ENABLED,
                R.string.analytics,
                0
            )
        )
        sl.add(
            RunRunnable(
                context,
                R.string.analytics_new_id,
                0,
                R.string.analytics_new_id_confirmation,
                0,
                true
            ) { NativeLibrary.GenerateNewStatisticsId() }
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_ENABLE_SAVESTATES,
                R.string.enable_save_states,
                R.string.enable_save_states_description
            )
        )
    }

    private fun addInterfaceSettings(sl: ArrayList<SettingsItem>) {
        // Hide the orientation setting if the device only supports one orientation. Old devices which
        // support both portrait and landscape may report support for neither, so we use ==, not &&.
        val packageManager = context.packageManager
        if (packageManager.hasSystemFeature(PackageManager.FEATURE_SCREEN_PORTRAIT) ==
            packageManager.hasSystemFeature(PackageManager.FEATURE_SCREEN_LANDSCAPE)
        ) {
            sl.add(
                SingleChoiceSetting(
                    context,
                    IntSetting.MAIN_EMULATION_ORIENTATION,
                    R.string.emulation_screen_orientation,
                    0,
                    R.array.orientationEntries,
                    R.array.orientationValues
                )
            )
        }

        // Only android 9+ supports this feature.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            sl.add(
                SwitchSetting(
                    context,
                    BooleanSetting.MAIN_EXPAND_TO_CUTOUT_AREA,
                    R.string.expand_to_cutout_area,
                    R.string.expand_to_cutout_area_description
                )
            )
        }

        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_USE_PANIC_HANDLERS,
                R.string.panic_handlers,
                R.string.panic_handlers_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_OSD_MESSAGES,
                R.string.osd_messages,
                R.string.osd_messages_description
            )
        )

        val appTheme: AbstractIntSetting = object : AbstractIntSetting {
            override val isOverridden: Boolean
                get() = IntSetting.MAIN_INTERFACE_THEME.isOverridden

            // This only affects app UI
            override val isRuntimeEditable: Boolean = true

            override fun delete(settings: Settings): Boolean {
                ThemeHelper.deleteThemeKey((fragmentView.fragmentActivity as AppCompatActivity))
                return IntSetting.MAIN_INTERFACE_THEME.delete(settings)
            }

            override val int: Int
                get() = IntSetting.MAIN_INTERFACE_THEME.int

            override fun setInt(settings: Settings, newValue: Int) {
                IntSetting.MAIN_INTERFACE_THEME.setInt(settings, newValue)
                ThemeHelper.saveTheme(
                    (fragmentView.fragmentActivity as AppCompatActivity),
                    newValue
                )
            }
        }

        // If a Monet theme is run on a device below API 31, the app will crash
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            sl.add(
                SingleChoiceSetting(
                    context,
                    appTheme,
                    R.string.change_theme,
                    0,
                    R.array.themeEntriesA12,
                    R.array.themeValuesA12
                )
            )
        } else {
            sl.add(
                SingleChoiceSetting(
                    context,
                    appTheme,
                    R.string.change_theme,
                    0,
                    R.array.themeEntries,
                    R.array.themeValues
                )
            )
        }

        val themeMode: AbstractIntSetting = object : AbstractIntSetting {
            override val isOverridden: Boolean
                get() = IntSetting.MAIN_INTERFACE_THEME_MODE.isOverridden

            // This only affects app UI
            override val isRuntimeEditable: Boolean = true

            override fun delete(settings: Settings): Boolean {
                ThemeHelper.deleteThemeModeKey((fragmentView.fragmentActivity as AppCompatActivity))
                return IntSetting.MAIN_INTERFACE_THEME_MODE.delete(settings)
            }

            override val int: Int
                get() = IntSetting.MAIN_INTERFACE_THEME_MODE.int

            override fun setInt(settings: Settings, newValue: Int) {
                IntSetting.MAIN_INTERFACE_THEME_MODE.setInt(settings, newValue)
                ThemeHelper.saveThemeMode(
                    (fragmentView.fragmentActivity as AppCompatActivity),
                    newValue
                )
            }
        }

        sl.add(
            SingleChoiceSetting(
                context,
                themeMode,
                R.string.change_theme_mode,
                0,
                R.array.themeModeEntries,
                R.array.themeModeValues
            )
        )

        val blackBackgrounds: AbstractBooleanSetting = object : AbstractBooleanSetting {
            override val isOverridden: Boolean
                get() = BooleanSetting.MAIN_USE_BLACK_BACKGROUNDS.isOverridden

            override val isRuntimeEditable: Boolean = true

            override fun delete(settings: Settings): Boolean {
                ThemeHelper.deleteBackgroundSetting((fragmentView.fragmentActivity as AppCompatActivity))
                return BooleanSetting.MAIN_USE_BLACK_BACKGROUNDS.delete(settings)
            }

            override val boolean: Boolean
                get() = BooleanSetting.MAIN_USE_BLACK_BACKGROUNDS.boolean

            override fun setBoolean(settings: Settings, newValue: Boolean) {
                BooleanSetting.MAIN_USE_BLACK_BACKGROUNDS.setBoolean(settings, newValue)
                ThemeHelper.saveBackgroundSetting(
                    (fragmentView.fragmentActivity as AppCompatActivity),
                    newValue
                )
            }
        }

        sl.add(
            SwitchSetting(
                context,
                blackBackgrounds,
                R.string.use_black_backgrounds,
                R.string.use_black_backgrounds_description
            )
        )
    }

    private fun addAudioSettings(sl: ArrayList<SettingsItem>) {
        val DSP_HLE = 0
        val DSP_LLE_RECOMPILER = 1
        val DSP_LLE_INTERPRETER = 2

        val dspEmulationEngine: AbstractIntSetting = object : AbstractIntSetting {
            override val int: Int
                get() = if (BooleanSetting.MAIN_DSP_HLE.boolean) {
                    DSP_HLE
                } else {
                    val jit = BooleanSetting.MAIN_DSP_JIT.boolean
                    if (jit) DSP_LLE_RECOMPILER else DSP_LLE_INTERPRETER
                }

            override fun setInt(settings: Settings, newValue: Int) {
                when (newValue) {
                    DSP_HLE -> {
                        BooleanSetting.MAIN_DSP_HLE.setBoolean(settings, true)
                        BooleanSetting.MAIN_DSP_JIT.setBoolean(settings, true)
                    }

                    DSP_LLE_RECOMPILER -> {
                        BooleanSetting.MAIN_DSP_HLE.setBoolean(settings, false)
                        BooleanSetting.MAIN_DSP_JIT.setBoolean(settings, true)
                    }

                    DSP_LLE_INTERPRETER -> {
                        BooleanSetting.MAIN_DSP_HLE.setBoolean(settings, false)
                        BooleanSetting.MAIN_DSP_JIT.setBoolean(settings, false)
                    }
                }
            }

            override val isOverridden: Boolean
                get() = BooleanSetting.MAIN_DSP_HLE.isOverridden ||
                        BooleanSetting.MAIN_DSP_JIT.isOverridden

            override val isRuntimeEditable: Boolean
                get() = BooleanSetting.MAIN_DSP_HLE.isRuntimeEditable &&
                        BooleanSetting.MAIN_DSP_JIT.isRuntimeEditable

            override fun delete(settings: Settings): Boolean {
                // Not short circuiting
                return BooleanSetting.MAIN_DSP_HLE.delete(settings) and
                        BooleanSetting.MAIN_DSP_JIT.delete(settings)
            }
        }

        // TODO: Exclude values from arrays instead of having multiple arrays.
        val defaultCpuCore = NativeLibrary.DefaultCPUCore()
        val dspEngineEntries: Int
        val dspEngineValues: Int
        if (defaultCpuCore == 1) {
            dspEngineEntries = R.array.dspEngineEntriesX86_64
            dspEngineValues = R.array.dspEngineValuesX86_64
        } else {
            dspEngineEntries = R.array.dspEngineEntriesGeneric
            dspEngineValues = R.array.dspEngineValuesGeneric
        }
        sl.add(
            SingleChoiceSetting(
                context,
                dspEmulationEngine,
                R.string.dsp_emulation_engine,
                0,
                dspEngineEntries,
                dspEngineValues
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_AUDIO_STRETCH,
                R.string.audio_stretch,
                R.string.audio_stretch_description
            )
        )
        sl.add(
            IntSliderSetting(
                context,
                IntSetting.MAIN_AUDIO_VOLUME,
                R.string.audio_volume,
                0,
                0,
                100,
                "%",
                1
            )
        )
    }

    private fun addPathsSettings(sl: ArrayList<SettingsItem>) {
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_RECURSIVE_ISO_PATHS,
                R.string.search_subfolders,
                0
            )
        )
        sl.add(
            FilePicker(
                context,
                StringSetting.MAIN_DEFAULT_ISO,
                R.string.default_ISO,
                0,
                MainPresenter.REQUEST_GAME_FILE,
                null
            )
        )
        sl.add(
            FilePicker(
                context,
                StringSetting.MAIN_FS_PATH,
                R.string.wii_NAND_root,
                0,
                MainPresenter.REQUEST_DIRECTORY,
                "/Wii"
            )
        )
        sl.add(
            FilePicker(
                context,
                StringSetting.MAIN_DUMP_PATH,
                R.string.dump_path,
                0,
                MainPresenter.REQUEST_DIRECTORY,
                "/Dump"
            )
        )
        sl.add(
            FilePicker(
                context,
                StringSetting.MAIN_LOAD_PATH,
                R.string.load_path,
                0,
                MainPresenter.REQUEST_DIRECTORY,
                "/Load"
            )
        )
        sl.add(
            FilePicker(
                context,
                StringSetting.MAIN_RESOURCEPACK_PATH,
                R.string.resource_pack_path,
                0,
                MainPresenter.REQUEST_DIRECTORY,
                "/ResourcePacks"
            )
        )
        sl.add(
            FilePicker(
                context,
                StringSetting.MAIN_WFS_PATH,
                R.string.wfs_path,
                0,
                MainPresenter.REQUEST_DIRECTORY,
                "/WFS"
            )
        )
    }

    private fun addGameCubeSettings(sl: ArrayList<SettingsItem>) {
        sl.add(HeaderSetting(context, R.string.ipl_settings, 0))
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_SKIP_IPL,
                R.string.skip_main_menu,
                R.string.skip_main_menu_description
            )
        )
        sl.add(
            SingleChoiceSetting(
                context,
                IntSetting.MAIN_GC_LANGUAGE,
                R.string.system_language,
                0,
                R.array.gameCubeSystemLanguageEntries,
                R.array.gameCubeSystemLanguageValues
            )
        )

        sl.add(HeaderSetting(context, R.string.device_settings, 0))
        sl.add(
            SingleChoiceSetting(
                context,
                IntSetting.MAIN_SLOT_A,
                R.string.slot_a_device,
                0,
                R.array.slotDeviceEntries,
                R.array.slotDeviceValues
            )
        )
        sl.add(
            SingleChoiceSetting(
                context,
                IntSetting.MAIN_SLOT_B,
                R.string.slot_b_device,
                0,
                R.array.slotDeviceEntries,
                R.array.slotDeviceValues
            )
        )
        sl.add(
            SingleChoiceSetting(
                context,
                IntSetting.MAIN_SERIAL_PORT_1,
                R.string.serial_port_1_device,
                0,
                R.array.serialPort1DeviceEntries,
                R.array.serialPort1DeviceValues,
                MenuTag.CONFIG_SERIALPORT1
            )
        )
    }

    private fun addWiiSettings(sl: ArrayList<SettingsItem>) {
        sl.add(HeaderSetting(context, R.string.wii_misc_settings, 0))
        sl.add(
            SingleChoiceSetting(
                context,
                IntSetting.SYSCONF_LANGUAGE,
                R.string.system_language,
                0,
                R.array.wiiSystemLanguageEntries,
                R.array.wiiSystemLanguageValues
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.SYSCONF_WIDESCREEN,
                R.string.wii_widescreen,
                R.string.wii_widescreen_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.SYSCONF_PAL60,
                R.string.wii_pal60,
                R.string.wii_pal60_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.SYSCONF_SCREENSAVER,
                R.string.wii_screensaver,
                R.string.wii_screensaver_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_WII_WIILINK_ENABLE,
                R.string.wii_enable_wiilink,
                R.string.wii_enable_wiilink_description
            )
        )
        sl.add(
            SingleChoiceSetting(
                context,
                IntSetting.SYSCONF_SOUND_MODE,
                R.string.sound_mode,
                0,
                R.array.soundModeEntries,
                R.array.soundModeValues
            )
        )

        sl.add(HeaderSetting(context, R.string.wii_sd_card_settings, 0))
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_WII_SD_CARD,
                R.string.insert_sd_card,
                R.string.insert_sd_card_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_ALLOW_SD_WRITES,
                R.string.wii_sd_card_allow_writes,
                0
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_WII_SD_CARD_ENABLE_FOLDER_SYNC,
                R.string.wii_sd_card_sync,
                R.string.wii_sd_card_sync_description
            )
        )
        // TODO: Hardcoding "Load" here is wrong, because the user may have changed the Load path.
        // The code structure makes this hard to fix, and with scoped storage active the Load path
        // can't be changed anyway
        sl.add(
            FilePicker(
                context,
                StringSetting.MAIN_WII_SD_CARD_IMAGE_PATH,
                R.string.wii_sd_card_path,
                0,
                MainPresenter.REQUEST_SD_FILE,
                "/Load/WiiSD.raw"
            )
        )
        sl.add(
            FilePicker(
                context,
                StringSetting.MAIN_WII_SD_CARD_SYNC_FOLDER_PATH,
                R.string.wii_sd_sync_folder,
                0,
                MainPresenter.REQUEST_DIRECTORY,
                "/Load/WiiSDSync/"
            )
        )
        sl.add(
            RunRunnable(
                context,
                R.string.wii_sd_card_folder_to_file,
                0,
                R.string.wii_sd_card_folder_to_file_confirmation,
                0,
                false
            ) { convertOnThread { WiiUtils.syncSdFolderToSdImage() } }
        )
        sl.add(
            RunRunnable(
                context,
                R.string.wii_sd_card_file_to_folder,
                0,
                R.string.wii_sd_card_file_to_folder_confirmation,
                0,
                false
            ) { convertOnThread { WiiUtils.syncSdImageToSdFolder() } }
        )

        sl.add(HeaderSetting(context, R.string.wii_wiimote_settings, 0))
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.SYSCONF_WIIMOTE_MOTOR,
                R.string.wiimote_rumble,
                0
            )
        )
        sl.add(
            IntSliderSetting(
                context,
                IntSetting.SYSCONF_SPEAKER_VOLUME,
                R.string.wiimote_volume,
                0,
                0,
                127,
                "",
                1
            )
        )
        sl.add(
            IntSliderSetting(
                context,
                IntSetting.SYSCONF_SENSOR_BAR_SENSITIVITY,
                R.string.sensor_bar_sensitivity,
                0,
                1,
                5,
                "",
                1
            )
        )
        sl.add(
            SingleChoiceSetting(
                context,
                IntSetting.SYSCONF_SENSOR_BAR_POSITION,
                R.string.sensor_bar_position,
                0,
                R.array.sensorBarPositionEntries,
                R.array.sensorBarPositionValues
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_WIIMOTE_CONTINUOUS_SCANNING,
                R.string.wiimote_scanning,
                R.string.wiimote_scanning_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_WIIMOTE_ENABLE_SPEAKER,
                R.string.wiimote_speaker,
                R.string.wiimote_speaker_description
            )
        )

        sl.add(HeaderSetting(context, R.string.emulated_usb_devices, 0))
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_EMULATE_SKYLANDER_PORTAL,
                R.string.emulate_skylander_portal,
                0
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_EMULATE_INFINITY_BASE,
                R.string.emulate_infinity_base,
                0
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_EMULATE_WII_SPEAK,
                R.string.emulate_wii_speak,
                0
            )
        )
        sl.add(
            InvertedSwitchSetting(
                context,
                BooleanSetting.MAIN_WII_SPEAK_CONNECTED,
                R.string.disconnect_wii_speak,
                0
            )
        )
    }

    private fun addAdvancedSettings(sl: ArrayList<SettingsItem>) {
        val SYNC_GPU_NEVER = 0
        val SYNC_GPU_ON_IDLE_SKIP = 1
        val SYNC_GPU_ALWAYS = 2

        val synchronizeGpuThread: AbstractIntSetting = object : AbstractIntSetting {
            override val int: Int
                get() = if (BooleanSetting.MAIN_SYNC_GPU.boolean) {
                    SYNC_GPU_ALWAYS
                } else {
                    val syncOnSkipIdle = BooleanSetting.MAIN_SYNC_ON_SKIP_IDLE.boolean
                    if (syncOnSkipIdle) SYNC_GPU_ON_IDLE_SKIP else SYNC_GPU_NEVER
                }

            override fun setInt(settings: Settings, newValue: Int) {
                when (newValue) {
                    SYNC_GPU_NEVER -> {
                        BooleanSetting.MAIN_SYNC_ON_SKIP_IDLE.setBoolean(settings, false)
                        BooleanSetting.MAIN_SYNC_GPU.setBoolean(settings, false)
                    }

                    SYNC_GPU_ON_IDLE_SKIP -> {
                        BooleanSetting.MAIN_SYNC_ON_SKIP_IDLE.setBoolean(settings, true)
                        BooleanSetting.MAIN_SYNC_GPU.setBoolean(settings, false)
                    }

                    SYNC_GPU_ALWAYS -> {
                        BooleanSetting.MAIN_SYNC_ON_SKIP_IDLE.setBoolean(settings, true)
                        BooleanSetting.MAIN_SYNC_GPU.setBoolean(settings, true)
                    }
                }
            }

            override val isOverridden: Boolean
                get() = BooleanSetting.MAIN_SYNC_ON_SKIP_IDLE.isOverridden ||
                        BooleanSetting.MAIN_SYNC_GPU.isOverridden

            override val isRuntimeEditable: Boolean
                get() = BooleanSetting.MAIN_SYNC_ON_SKIP_IDLE.isRuntimeEditable &&
                        BooleanSetting.MAIN_SYNC_GPU.isRuntimeEditable

            override fun delete(settings: Settings): Boolean {
                // Not short circuiting
                return BooleanSetting.MAIN_SYNC_ON_SKIP_IDLE.delete(settings) and
                        BooleanSetting.MAIN_SYNC_GPU.delete(settings)
            }
        }

        // TODO: Having different emuCoresEntries/emuCoresValues for each architecture is annoying.
        //       The proper solution would be to have one set of entries and one set of values
        //       and exclude the values that aren't present in PowerPC::AvailableCPUCores().
        val defaultCpuCore = NativeLibrary.DefaultCPUCore()
        val emuCoresEntries: Int
        val emuCoresValues: Int
        when (defaultCpuCore) {
            1 -> {
                emuCoresEntries = R.array.emuCoresEntriesX86_64
                emuCoresValues = R.array.emuCoresValuesX86_64
            }

            4 -> {
                emuCoresEntries = R.array.emuCoresEntriesARM64
                emuCoresValues = R.array.emuCoresValuesARM64
            }

            else -> {
                emuCoresEntries = R.array.emuCoresEntriesGeneric
                emuCoresValues = R.array.emuCoresValuesGeneric
            }
        }

        sl.add(HeaderSetting(context, R.string.cpu_options, 0))
        sl.add(
            SingleChoiceSetting(
                context,
                IntSetting.MAIN_CPU_CORE,
                R.string.cpu_core,
                0,
                emuCoresEntries,
                emuCoresValues
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_MMU,
                R.string.mmu_enable,
                R.string.mmu_enable_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_PAUSE_ON_PANIC,
                R.string.pause_on_panic,
                R.string.pause_on_panic_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_ACCURATE_CPU_CACHE,
                R.string.enable_cpu_cache,
                R.string.enable_cpu_cache_description
            )
        )

        sl.add(HeaderSetting(context, R.string.clock_override, 0))
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_OVERCLOCK_ENABLE,
                R.string.overclock_enable,
                R.string.overclock_enable_description
            )
        )
        sl.add(
            PercentSliderSetting(
                context,
                FloatSetting.MAIN_OVERCLOCK,
                R.string.overclock_title,
                R.string.overclock_title_description,
                0f,
                400f,
                "%",
                1f,
                false
            )
        )

        val mem1Size = ScaledIntSetting(1024 * 1024, IntSetting.MAIN_MEM1_SIZE)
        val mem2Size = ScaledIntSetting(1024 * 1024, IntSetting.MAIN_MEM2_SIZE)

        sl.add(HeaderSetting(context, R.string.memory_override, 0))
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_RAM_OVERRIDE_ENABLE,
                R.string.enable_memory_size_override,
                R.string.enable_memory_size_override_description
            )
        )
        sl.add(
            IntSliderSetting(
                context,
                mem1Size,
                R.string.main_mem1_size,
                0,
                24,
                64,
                "MB",
                1
            )
        )
        sl.add(
            IntSliderSetting(
                context,
                mem2Size,
                R.string.main_mem2_size,
                0,
                64,
                128,
                "MB",
                1
            )
        )

        sl.add(HeaderSetting(context, R.string.gpu_options, 0))
        sl.add(
            SingleChoiceSetting(
                context,
                synchronizeGpuThread,
                R.string.synchronize_gpu_thread,
                R.string.synchronize_gpu_thread_description,
                R.array.synchronizeGpuThreadEntries,
                R.array.synchronizeGpuThreadValues
            )
        )

        sl.add(HeaderSetting(context, R.string.custom_rtc_options, 0))
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_CUSTOM_RTC_ENABLE,
                R.string.custom_rtc_enable,
                R.string.custom_rtc_description
            )
        )
        sl.add(
            DateTimeChoiceSetting(
                context,
                StringSetting.MAIN_CUSTOM_RTC_VALUE,
                R.string.set_custom_rtc,
                0
            )
        )

        sl.add(HeaderSetting(context, R.string.misc_settings, 0))
        sl.add(
            InvertedSwitchSetting(
                context,
                BooleanSetting.MAIN_FAST_DISC_SPEED,
                R.string.emulate_disc_speed,
                R.string.emulate_disc_speed_description
            )
        )
    }

    private fun addSerialPortSubSettings(sl: ArrayList<SettingsItem>, serialPort1Type: Int) {
        if (serialPort1Type == 10) {
            // Broadband Adapter (XLink Kai)
            sl.add(HyperLinkHeaderSetting(context, R.string.xlink_kai_guide_header, 0))
            sl.add(
                InputStringSetting(
                    context,
                    StringSetting.MAIN_BBA_XLINK_IP,
                    R.string.xlink_kai_bba_ip,
                    R.string.xlink_kai_bba_ip_description
                )
            )
        } else if (serialPort1Type == 11) {
            // Broadband Adapter (tapserver)
            sl.add(
                InputStringSetting(
                    context,
                    StringSetting.MAIN_BBA_TAPSERVER_DESTINATION,
                    R.string.bba_tapserver_destination,
                    R.string.bba_tapserver_destination_description
                )
            )
        } else if (serialPort1Type == 12) {
            // Broadband Adapter (Built In)
            sl.add(
                InputStringSetting(
                    context,
                    StringSetting.MAIN_BBA_BUILTIN_DNS,
                    R.string.bba_builtin_dns,
                    R.string.bba_builtin_dns_description
                )
            )
        } else if (serialPort1Type == 13) {
            // Modem Adapter (tapserver)
            sl.add(
                InputStringSetting(
                    context,
                    StringSetting.MAIN_MODEM_TAPSERVER_DESTINATION,
                    R.string.modem_tapserver_destination,
                    R.string.modem_tapserver_destination_description
                )
            )
        }
    }

    private fun addGcPadSettings(sl: ArrayList<SettingsItem>) {
        sl.add(
            SingleChoiceSetting(
                context,
                IntSetting.MAIN_SI_DEVICE_0,
                R.string.controller_0,
                0,
                R.array.gcpadTypeEntries,
                R.array.gcpadTypeValues,
                MenuTag.getGCPadMenuTag(0)
            )
        )
        sl.add(
            SingleChoiceSetting(
                context,
                IntSetting.MAIN_SI_DEVICE_1,
                R.string.controller_1,
                0,
                R.array.gcpadTypeEntries,
                R.array.gcpadTypeValues,
                MenuTag.getGCPadMenuTag(1)
            )
        )
        sl.add(
            SingleChoiceSetting(
                context,
                IntSetting.MAIN_SI_DEVICE_2,
                R.string.controller_2,
                0,
                R.array.gcpadTypeEntries,
                R.array.gcpadTypeValues,
                MenuTag.getGCPadMenuTag(2)
            )
        )
        sl.add(
            SingleChoiceSetting(
                context,
                IntSetting.MAIN_SI_DEVICE_3,
                R.string.controller_3,
                0,
                R.array.gcpadTypeEntries,
                R.array.gcpadTypeValues,
                MenuTag.getGCPadMenuTag(3)
            )
        )
    }

    private fun addWiimoteSettings(sl: ArrayList<SettingsItem>) {
        sl.add(
            SingleChoiceSetting(
                context,
                IntSetting.WIIMOTE_1_SOURCE,
                R.string.wiimote_0,
                0,
                R.array.wiimoteTypeEntries,
                R.array.wiimoteTypeValues,
                MenuTag.getWiimoteMenuTag(0)
            )
        )
        sl.add(
            SingleChoiceSetting(
                context,
                IntSetting.WIIMOTE_2_SOURCE,
                R.string.wiimote_1,
                0,
                R.array.wiimoteTypeEntries,
                R.array.wiimoteTypeValues,
                MenuTag.getWiimoteMenuTag(1)
            )
        )
        sl.add(
            SingleChoiceSetting(
                context,
                IntSetting.WIIMOTE_3_SOURCE,
                R.string.wiimote_2,
                0,
                R.array.wiimoteTypeEntries,
                R.array.wiimoteTypeValues,
                MenuTag.getWiimoteMenuTag(2)
            )
        )
        sl.add(
            SingleChoiceSetting(
                context,
                IntSetting.WIIMOTE_4_SOURCE,
                R.string.wiimote_3,
                0,
                R.array.wiimoteTypeEntries,
                R.array.wiimoteTypeValues,
                MenuTag.getWiimoteMenuTag(3)
            )
        )
    }

    private fun addGraphicsSettings(sl: ArrayList<SettingsItem>) {
        sl.add(HeaderSetting(context, R.string.graphics_general, 0))
        sl.add(
            StringSingleChoiceSetting(
                context,
                StringSetting.MAIN_GFX_BACKEND,
                R.string.video_backend,
                0,
                R.array.videoBackendEntries,
                R.array.videoBackendValues
            )
        )
        sl.add(
            SingleChoiceSettingDynamicDescriptions(
                context,
                IntSetting.GFX_SHADER_COMPILATION_MODE,
                R.string.shader_compilation_mode,
                0,
                R.array.shaderCompilationModeEntries,
                R.array.shaderCompilationModeValues,
                R.array.shaderCompilationDescriptionEntries,
                R.array.shaderCompilationDescriptionValues
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_WAIT_FOR_SHADERS_BEFORE_STARTING,
                R.string.wait_for_shaders,
                R.string.wait_for_shaders_description
            )
        )
        sl.add(
            SingleChoiceSetting(
                context,
                IntSetting.GFX_ASPECT_RATIO,
                R.string.aspect_ratio,
                0,
                R.array.aspectRatioEntries,
                R.array.aspectRatioValues
            )
        )

        sl.add(HeaderSetting(context, R.string.graphics_more_settings, 0))
        sl.add(
            SubmenuSetting(
                context,
                R.string.enhancements_submenu,
                MenuTag.ENHANCEMENTS
            )
        )
        sl.add(
            SubmenuSetting(
                context,
                R.string.hacks_submenu,
                MenuTag.HACKS
            )
        )
        sl.add(
            SubmenuSetting(
                context,
                R.string.statistics_submenu,
                MenuTag.STATISTICS
            )
        )
        sl.add(
            SubmenuSetting(
                context,
                R.string.advanced_graphics_submenu,
                MenuTag.ADVANCED_GRAPHICS
            )
        )

        if (
            this.gpuDriver != null && this.gameId.isNullOrEmpty()
            && !NativeLibrary.IsRunning()
            && GpuDriverHelper.supportsCustomDriverLoading()
        ) {
            sl.add(
                SubmenuSetting(
                    context,
                    R.string.gpu_driver_submenu, MenuTag.GPU_DRIVERS
                )
            )
        }
    }

    private fun addEnhanceSettings(sl: ArrayList<SettingsItem>) {
        sl.add(
            SingleChoiceSetting(
                context,
                IntSetting.GFX_EFB_SCALE,
                R.string.internal_resolution,
                R.string.internal_resolution_description,
                R.array.internalResolutionEntries,
                R.array.internalResolutionValues
            )
        )
        sl.add(
            SingleChoiceSetting(
                context,
                IntSetting.GFX_MSAA,
                R.string.FSAA,
                R.string.FSAA_description,
                R.array.FSAAEntries,
                R.array.FSAAValues
            )
        )
        sl.add(
            SingleChoiceSetting(
                context,
                IntSetting.GFX_ENHANCE_MAX_ANISOTROPY,
                R.string.anisotropic_filtering,
                R.string.anisotropic_filtering_description,
                R.array.anisotropicFilteringEntries,
                R.array.anisotropicFilteringValues
            )
        )
        sl.add(
            SingleChoiceSetting(
                context,
                IntSetting.GFX_ENHANCE_FORCE_TEXTURE_FILTERING,
                R.string.texture_filtering,
                R.string.texture_filtering_description,
                R.array.textureFilteringEntries,
                R.array.textureFilteringValues
            )
        )
        sl.add(
            SubmenuSetting(
                context,
                R.string.color_correction_submenu,
                MenuTag.COLOR_CORRECTION
            )
        )

        val stereoModeValue = IntSetting.GFX_STEREO_MODE.int
        val anaglyphMode = 3
        val shaderList =
            if (stereoModeValue == anaglyphMode) PostProcessing.anaglyphShaderList else PostProcessing.shaderList

        val shaderListEntries = arrayOf(context.getString(R.string.off), *shaderList)
        val shaderListValues = arrayOf("", *shaderList)

        sl.add(
            StringSingleChoiceSetting(
                context,
                StringSetting.GFX_ENHANCE_POST_SHADER,
                R.string.post_processing_shader,
                0,
                shaderListEntries,
                shaderListValues
            )
        )

        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_HACK_COPY_EFB_SCALED,
                R.string.scaled_efb_copy,
                R.string.scaled_efb_copy_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_ENABLE_PIXEL_LIGHTING,
                R.string.per_pixel_lighting,
                R.string.per_pixel_lighting_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_ENHANCE_FORCE_TRUE_COLOR,
                R.string.force_24bit_color,
                R.string.force_24bit_color_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_DISABLE_FOG,
                R.string.disable_fog,
                R.string.disable_fog_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_ENHANCE_DISABLE_COPY_FILTER,
                R.string.disable_copy_filter,
                R.string.disable_copy_filter_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION,
                R.string.arbitrary_mipmap_detection,
                R.string.arbitrary_mipmap_detection_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_WIDESCREEN_HACK,
                R.string.wide_screen_hack,
                R.string.wide_screen_hack_description
            )
        )

        // Check if we support stereo
        // If we support desktop GL then we must support at least OpenGL 3.2
        // If we only support OpenGLES then we need both OpenGLES 3.1 and AEP
        val helper = EGLHelper(EGLHelper.EGL_OPENGL_ES2_BIT)

        if (helper.supportsOpenGL() && helper.GetVersion() >= 320 || helper.supportsGLES3() && helper.GetVersion() >= 310 && helper.SupportsExtension(
                "GL_ANDROID_extension_pack_es31a"
            )
        ) {
            sl.add(
                SubmenuSetting(
                    context,
                    R.string.stereoscopy_submenu,
                    MenuTag.STEREOSCOPY
                )
            )
        }
    }

    private fun addColorCorrectionSettings(sl: ArrayList<SettingsItem>) {
        sl.apply {
            add(HeaderSetting(context, R.string.color_space, 0))
            add(
                SwitchSetting(
                    context,
                    BooleanSetting.GFX_CC_CORRECT_COLOR_SPACE,
                    R.string.correct_color_space,
                    R.string.correct_color_space_description
                )
            )
            add(
                SingleChoiceSetting(
                    context,
                    IntSetting.GFX_CC_GAME_COLOR_SPACE,
                    R.string.game_color_space,
                    0,
                    R.array.colorSpaceEntries,
                    R.array.colorSpaceValues
                )
            )

            add(HeaderSetting(context, R.string.gamma, 0))
            add(
                FloatSliderSetting(
                    context,
                    FloatSetting.GFX_CC_GAME_GAMMA,
                    R.string.game_gamma,
                    R.string.game_gamma_description,
                    2.2f,
                    2.8f,
                    "",
                    0.01f,
                    true
                )
            )
            add(
                SwitchSetting(
                    context,
                    BooleanSetting.GFX_CC_CORRECT_GAMMA,
                    R.string.correct_sdr_gamma,
                    0
                )
            )
        }
    }

    private fun addHackSettings(sl: ArrayList<SettingsItem>) {
        sl.add(HeaderSetting(context, R.string.embedded_frame_buffer, 0))
        sl.add(
            InvertedSwitchSetting(
                context,
                BooleanSetting.GFX_HACK_EFB_ACCESS_ENABLE,
                R.string.skip_efb_access,
                R.string.skip_efb_access_description
            )
        )
        sl.add(
            InvertedSwitchSetting(
                context,
                BooleanSetting.GFX_HACK_EFB_EMULATE_FORMAT_CHANGES,
                R.string.ignore_format_changes,
                R.string.ignore_format_changes_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_HACK_SKIP_EFB_COPY_TO_RAM,
                R.string.efb_copy_method,
                R.string.efb_copy_method_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_HACK_DEFER_EFB_COPIES,
                R.string.defer_efb_copies,
                R.string.defer_efb_copies_description
            )
        )

        sl.add(HeaderSetting(context, R.string.texture_cache, 0))
        sl.add(
            SingleChoiceSetting(
                context,
                IntSetting.GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES,
                R.string.texture_cache_accuracy,
                R.string.texture_cache_accuracy_description,
                R.array.textureCacheAccuracyEntries,
                R.array.textureCacheAccuracyValues
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_ENABLE_GPU_TEXTURE_DECODING,
                R.string.gpu_texture_decoding,
                R.string.gpu_texture_decoding_description
            )
        )

        sl.add(HeaderSetting(context, R.string.external_frame_buffer, 0))
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_HACK_SKIP_XFB_COPY_TO_RAM,
                R.string.xfb_copy_method,
                R.string.xfb_copy_method_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_HACK_IMMEDIATE_XFB,
                R.string.immediate_xfb,
                R.string.immediate_xfb_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_HACK_SKIP_DUPLICATE_XFBS,
                R.string.skip_duplicate_xfbs,
                R.string.skip_duplicate_xfbs_description
            )
        )

        sl.add(HeaderSetting(context, R.string.other, 0))
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_FAST_DEPTH_CALC,
                R.string.fast_depth_calculation,
                R.string.fast_depth_calculation_description
            )
        )
        sl.add(
            InvertedSwitchSetting(
                context,
                BooleanSetting.GFX_HACK_BBOX_ENABLE,
                R.string.disable_bbox,
                R.string.disable_bbox_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_HACK_VERTEX_ROUNDING,
                R.string.vertex_rounding,
                R.string.vertex_rounding_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_HACK_VI_SKIP,
                R.string.vi_skip,
                R.string.vi_skip_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_SAVE_TEXTURE_CACHE_TO_STATE,
                R.string.texture_cache_to_state,
                R.string.texture_cache_to_state_description
            )
        )
    }

    private fun addStatisticsSettings(sl: ArrayList<SettingsItem>) {
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_SHOW_FPS,
                R.string.show_fps,
                R.string.show_fps_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_SHOW_FTIMES,
                R.string.show_ftimes,
                R.string.show_ftimes_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_SHOW_VPS,
                R.string.show_vps,
                R.string.show_vps_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_SHOW_VTIMES,
                R.string.show_vtimes,
                R.string.show_vtimes_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_SHOW_GRAPHS,
                R.string.show_graphs,
                R.string.show_graphs_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_SHOW_SPEED,
                R.string.show_speed,
                R.string.show_speed_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_SHOW_SPEED_COLORS,
                R.string.show_speed_colors,
                R.string.show_speed_colors_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_LOG_RENDER_TIME_TO_FILE,
                R.string.log_render_time_to_file,
                R.string.log_render_time_to_file_description
            )
        )
        sl.add(
            IntSliderSetting(
                context,
                IntSetting.GFX_PERF_SAMP_WINDOW,
                R.string.performance_sample_window,
                R.string.performance_sample_window_description,
                0,
                10000,
                "ms",
                100
            )
        )
    }

    private fun addAdvancedGraphicsSettings(sl: ArrayList<SettingsItem>) {
        sl.add(HeaderSetting(context, R.string.gfx_mods_and_custom_textures, 0))
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_MODS_ENABLE,
                R.string.gfx_mods,
                R.string.gfx_mods_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_HIRES_TEXTURES,
                R.string.load_custom_texture,
                R.string.load_custom_texture_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_CACHE_HIRES_TEXTURES,
                R.string.cache_custom_texture,
                R.string.cache_custom_texture_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_DUMP_TEXTURES,
                R.string.dump_texture,
                R.string.dump_texture_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_DUMP_BASE_TEXTURES,
                R.string.dump_base_texture,
                R.string.dump_base_texture_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_DUMP_MIP_TEXTURES,
                R.string.dump_mip_texture,
                R.string.dump_mip_texture_description
            )
        )

        sl.add(HeaderSetting(context, R.string.misc, 0))
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_CROP,
                R.string.crop,
                R.string.crop_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.SYSCONF_PROGRESSIVE_SCAN,
                R.string.progressive_scan,
                0
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_BACKEND_MULTITHREADING,
                R.string.backend_multithreading,
                R.string.backend_multithreading_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_PREFER_VS_FOR_LINE_POINT_EXPANSION,
                R.string.prefer_vs_for_point_line_expansion,
                R.string.prefer_vs_for_point_line_expansion_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_CPU_CULL,
                R.string.cpu_cull,
                R.string.cpu_cull_description
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_HACK_EFB_DEFER_INVALIDATION,
                R.string.defer_efb_invalidation,
                R.string.defer_efb_invalidation_description
            )
        )
        sl.add(
            InvertedSwitchSetting(
                context,
                BooleanSetting.GFX_HACK_FAST_TEXTURE_SAMPLING,
                R.string.manual_texture_sampling,
                R.string.manual_texture_sampling_description
            )
        )

        sl.add(HeaderSetting(context, R.string.frame_dumping, 0))
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_INTERNAL_RESOLUTION_FRAME_DUMPS,
                R.string.internal_resolution_dumps,
                R.string.internal_resolution_dumps_description
            )
        )
        sl.add(
            IntSliderSetting(
                context,
                IntSetting.GFX_PNG_COMPRESSION_LEVEL,
                R.string.png_compression_level,
                0,
                0,
                9,
                "",
                1
            )
        )

        sl.add(HeaderSetting(context, R.string.debugging, 0))
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_ENABLE_WIREFRAME,
                R.string.wireframe,
                R.string.leave_this_unchecked
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_OVERLAY_STATS,
                R.string.show_stats,
                R.string.leave_this_unchecked
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_TEXFMT_OVERLAY_ENABLE,
                R.string.texture_format,
                R.string.leave_this_unchecked
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_ENABLE_VALIDATION_LAYER,
                R.string.validation_layer,
                R.string.leave_this_unchecked
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_DUMP_EFB_TARGET,
                R.string.dump_efb,
                R.string.leave_this_unchecked
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_DUMP_XFB_TARGET,
                R.string.dump_xfb,
                R.string.leave_this_unchecked
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_HACK_DISABLE_COPY_TO_VRAM,
                R.string.disable_vram_copies,
                R.string.leave_this_unchecked
            )
        )
    }

    private fun addLogConfigurationSettings(sl: ArrayList<SettingsItem>) {
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.LOGGER_WRITE_TO_FILE,
                R.string.log_to_file,
                R.string.log_to_file_description
            )
        )
        sl.add(
            SingleChoiceSetting(
                context,
                IntSetting.LOGGER_VERBOSITY,
                R.string.log_verbosity,
                0,
                logVerbosityEntries, logVerbosityValues
            )
        )
        sl.add(
            RunRunnable(
                context,
                R.string.log_enable_all,
                0,
                R.string.log_enable_all_confirmation,
                0,
                true
            ) { setAllLogTypes(true) })
        sl.add(
            RunRunnable(
                context,
                R.string.log_disable_all,
                0,
                R.string.log_disable_all_confirmation,
                0,
                true
            ) { setAllLogTypes(false) })
        sl.add(
            RunRunnable(
                context,
                R.string.log_clear,
                0,
                R.string.log_clear_confirmation,
                0,
                true
            ) { SettingsAdapter.clearLog() })

        sl.add(HeaderSetting(context, R.string.log_types, 0))
        for ((key, value) in LOG_TYPE_NAMES) {
            sl.add(LogSwitchSetting(key, value, ""))
        }
    }

    private fun addDebugSettings(sl: ArrayList<SettingsItem>) {
        sl.add(HeaderSetting(context, R.string.debug_warning, 0))
        sl.add(
            InvertedSwitchSetting(
                context,
                BooleanSetting.MAIN_FASTMEM,
                R.string.debug_fastmem,
                0
            )
        )
        sl.add(
            InvertedSwitchSetting(
                context,
                BooleanSetting.MAIN_FASTMEM_ARENA,
                R.string.debug_fastmem_arena,
                0
            )
        )
        sl.add(
            InvertedSwitchSetting(
                context,
                BooleanSetting.MAIN_LARGE_ENTRY_POINTS_MAP,
                R.string.debug_large_entry_points_map,
                0
            )
        )

        sl.add(HeaderSetting(context, R.string.debug_jit_profiling_header, 0))
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_DEBUG_JIT_ENABLE_PROFILING,
                R.string.debug_jit_enable_block_profiling,
                0
           )
        )
        sl.add(
            RunRunnable(
                context,
                R.string.debug_jit_write_block_log_dump,
                0,
                0,
                0,
                true
            ) { NativeLibrary.WriteJitBlockLogDump() }
        )

        sl.add(HeaderSetting(context, R.string.debug_jit_header, 0))
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_DEBUG_JIT_OFF,
                R.string.debug_jitoff,
                0
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_DEBUG_JIT_LOAD_STORE_OFF,
                R.string.debug_jitloadstoreoff,
                0
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_DEBUG_JIT_LOAD_STORE_FLOATING_OFF,
                R.string.debug_jitloadstorefloatingoff,
                0
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_DEBUG_JIT_LOAD_STORE_PAIRED_OFF,
                R.string.debug_jitloadstorepairedoff,
                0
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_DEBUG_JIT_FLOATING_POINT_OFF,
                R.string.debug_jitfloatingpointoff,
                0
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_DEBUG_JIT_INTEGER_OFF,
                R.string.debug_jitintegeroff,
                0
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_DEBUG_JIT_PAIRED_OFF,
                R.string.debug_jitpairedoff,
                0
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_DEBUG_JIT_SYSTEM_REGISTERS_OFF,
                R.string.debug_jitsystemregistersoff,
                0
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_DEBUG_JIT_BRANCH_OFF,
                R.string.debug_jitbranchoff,
                0
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.MAIN_DEBUG_JIT_REGISTER_CACHE_OFF,
                R.string.debug_jitregistercacheoff,
                0
            )
        )
    }

    private fun addStereoSettings(sl: ArrayList<SettingsItem>) {
        sl.add(
            SingleChoiceSetting(
                context,
                IntSetting.GFX_STEREO_MODE,
                R.string.stereoscopy_mode,
                0,
                R.array.stereoscopyEntries,
                R.array.stereoscopyValues
            )
        )
        sl.add(
            IntSliderSetting(
                context,
                IntSetting.GFX_STEREO_DEPTH,
                R.string.stereoscopy_depth,
                R.string.stereoscopy_depth_description,
                0,
                100,
                "%",
                1
            )
        )
        sl.add(
            IntSliderSetting(
                context,
                IntSetting.GFX_STEREO_CONVERGENCE_PERCENTAGE,
                R.string.stereoscopy_convergence,
                R.string.stereoscopy_convergence_description,
                0,
                200,
                "%",
                1
            )
        )
        sl.add(
            SwitchSetting(
                context,
                BooleanSetting.GFX_STEREO_SWAP_EYES,
                R.string.stereoscopy_swap_eyes,
                R.string.stereoscopy_swap_eyes_description
            )
        )
    }

    private fun addGcPadSubSettings(sl: ArrayList<SettingsItem>, gcPadNumber: Int, gcPadType: Int) {
        when (gcPadType) {
            6, 8, 9, 10 -> {
                // Emulated
                val gcPad = EmulatedController.getGcPad(gcPadNumber)

                if (!TextUtils.isEmpty(gameId)) {
                    addControllerPerGameSettings(sl, gcPad, gcPadNumber)
                } else {
                    addControllerMetaSettings(sl, gcPad)
                    addControllerMappingSettings(sl, gcPad, null)
                }
            }
            7 -> {
                // Emulated keyboard controller
                val gcKeyboard = EmulatedController.getGcKeyboard(gcPadNumber)

                if (!TextUtils.isEmpty(gameId)) {
                    addControllerPerGameSettings(sl, gcKeyboard, gcPadNumber)
                } else {
                    sl.add(HeaderSetting(context, R.string.keyboard_controller_warning, 0))
                    addControllerMetaSettings(sl, gcKeyboard)
                    addControllerMappingSettings(sl, gcKeyboard, null)
                }
            }
            12 -> {
                // Adapter
                sl.add(
                    SwitchSetting(
                        context,
                        BooleanSetting.getSettingForAdapterRumble(gcPadNumber),
                        R.string.gc_adapter_rumble,
                        R.string.gc_adapter_rumble_description
                    )
                )
                sl.add(
                    SwitchSetting(
                        context,
                        BooleanSetting.getSettingForSimulateKonga(gcPadNumber),
                        R.string.gc_adapter_bongos,
                        R.string.gc_adapter_bongos_description
                    )
                )
            }
        }
    }

    private fun addWiimoteSubSettings(sl: ArrayList<SettingsItem>, wiimoteNumber: Int) {
        val wiimote = EmulatedController.getWiimote(wiimoteNumber)

        if (!TextUtils.isEmpty(gameId)) {
            addControllerPerGameSettings(sl, wiimote, wiimoteNumber)
        } else {
            addControllerMetaSettings(sl, wiimote)

            sl.add(HeaderSetting(context, R.string.wiimote, 0))
            sl.add(
                SubmenuSetting(
                    context,
                    R.string.wiimote_general,
                    MenuTag.getWiimoteGeneralMenuTag(wiimoteNumber)
                )
            )
            sl.add(
                SubmenuSetting(
                    context,
                    R.string.wiimote_motion_simulation,
                    MenuTag.getWiimoteMotionSimulationMenuTag(wiimoteNumber)
                )
            )
            sl.add(
                SubmenuSetting(
                    context,
                    R.string.wiimote_motion_input,
                    MenuTag.getWiimoteMotionInputMenuTag(wiimoteNumber)
                )
            )

            // TYPE_OTHER is included here instead of in addWiimoteGeneralSubSettings so that touchscreen
            // users won't have to dig into a submenu to find the Sideways Wii Remote setting
            addControllerMappingSettings(
                sl,
                wiimote,
                ArraySet(listOf(ControlGroup.TYPE_ATTACHMENTS, ControlGroup.TYPE_OTHER))
            )
        }
    }

    private fun addExtensionTypeSettings(
        sl: ArrayList<SettingsItem>,
        wiimoteNumber: Int,
        extensionType: Int
    ) {
        addControllerMappingSettings(
            sl,
            EmulatedController.getWiimoteAttachment(wiimoteNumber, extensionType),
            null
        )
    }

    private fun addWiimoteGeneralSubSettings(sl: ArrayList<SettingsItem>, wiimoteNumber: Int) {
        addControllerMappingSettings(
            sl,
            EmulatedController.getWiimote(wiimoteNumber),
            setOf(ControlGroup.TYPE_BUTTONS)
        )
    }

    private fun addWiimoteMotionSimulationSubSettings(
        sl: ArrayList<SettingsItem>,
        wiimoteNumber: Int
    ) {
        addControllerMappingSettings(
            sl, EmulatedController.getWiimote(wiimoteNumber),
            ArraySet(
                listOf(
                    ControlGroup.TYPE_FORCE,
                    ControlGroup.TYPE_TILT,
                    ControlGroup.TYPE_CURSOR,
                    ControlGroup.TYPE_SHAKE
                )
            )
        )
    }

    private fun addWiimoteMotionInputSubSettings(sl: ArrayList<SettingsItem>, wiimoteNumber: Int) {
        addControllerMappingSettings(
            sl, EmulatedController.getWiimote(wiimoteNumber),
            ArraySet(
                listOf(
                    ControlGroup.TYPE_IMU_ACCELEROMETER,
                    ControlGroup.TYPE_IMU_GYROSCOPE,
                    ControlGroup.TYPE_IMU_CURSOR
                )
            )
        )
    }

    /**
     * Adds controller settings that can be set on a per-game basis.
     *
     * @param sl               The list to place controller settings into.
     * @param profileString    The prefix used for the profile setting in game INI files.
     * @param controllerNumber The index of the controller, 0-3.
     */
    private fun addControllerPerGameSettings(
        sl: ArrayList<SettingsItem>,
        controller: EmulatedController,
        controllerNumber: Int
    ) {
        val profiles = ProfileDialogPresenter(menuTag).getProfileNames(false)
        val profileKey = controller.getProfileKey() + "Profile" + (controllerNumber + 1)
        sl.add(
            StringSingleChoiceSetting(
                context,
                AdHocStringSetting(Settings.FILE_GAME_SETTINGS_ONLY, "Controls", profileKey, ""),
                R.string.input_profile,
                0,
                profiles,
                profiles,
                R.string.input_profiles_empty
            )
        )
    }

    /**
     * Adds settings and actions that apply to a controller as a whole.
     * For instance, the device setting and the Clear action.
     *
     * @param sl         The list to place controller settings into.
     * @param controller The controller to add settings for.
     */
    private fun addControllerMetaSettings(
        sl: ArrayList<SettingsItem>,
        controller: EmulatedController
    ) {
        sl.add(
            InputDeviceSetting(
                context,
                R.string.input_device,
                0,
                controller
            )
        )

        sl.add(SwitchSetting(context, object : AbstractBooleanSetting {
            override val isOverridden: Boolean = false

            override val isRuntimeEditable: Boolean = true

            override fun delete(settings: Settings): Boolean {
                fragmentView.isMappingAllDevices = false
                return true
            }

            override val boolean: Boolean
                get() = fragmentView.isMappingAllDevices

            override fun setBoolean(settings: Settings, newValue: Boolean) {
                fragmentView.isMappingAllDevices = newValue
            }
        }, R.string.input_device_all_devices, R.string.input_device_all_devices_description))

        sl.add(
            RunRunnable(
                context,
                R.string.input_reset_to_default,
                R.string.input_reset_to_default_description,
                R.string.input_reset_warning,
                0,
                true
            ) { loadDefaultControllerSettings(controller) })
        sl.add(
            RunRunnable(
                context,
                R.string.input_clear,
                R.string.input_clear_description,
                R.string.input_reset_warning,
                0,
                true
            ) { clearControllerSettings(controller) })
        sl.add(
            RunRunnable(
                context,
                R.string.input_profiles,
                0,
                0,
                0,
                true
            ) { fragmentView.showDialogFragment(ProfileDialog.create(menuTag)) })

        updateOldControllerSettingsWarningVisibility(controller)
    }

    /**
     * Adds mapping settings and other control-specific settings.
     *
     * @param sl              The list to place controller settings into.
     * @param controller      The controller to add settings for.
     * @param groupTypeFilter If this is non-null, only groups whose types match this are considered.
     */
    private fun addControllerMappingSettings(
        sl: ArrayList<SettingsItem>,
        controller: EmulatedController,
        groupTypeFilter: Set<Int>?
    ) {
        updateOldControllerSettingsWarningVisibility(controller)

        val groupCount = controller.getGroupCount()
        for (i in 0 until groupCount) {
            val group = controller.getGroup(i)
            val groupType = group.getGroupType()
            if (groupTypeFilter != null && !groupTypeFilter.contains(groupType)) continue

            sl.add(HeaderSetting(group.getUiName(), ""))

            if (group.getDefaultEnabledValue() != ControlGroup.DEFAULT_ENABLED_ALWAYS) {
                sl.add(
                    SwitchSetting(
                        context,
                        ControlGroupEnabledSetting(group),
                        R.string.enabled,
                        0
                    )
                )
            }

            val controlCount = group.getControlCount()
            for (j in 0 until controlCount) {
                sl.add(InputMappingControlSetting(group.getControl(j), controller))
            }

            if (groupType == ControlGroup.TYPE_ATTACHMENTS) {
                val attachmentSetting = group.getAttachmentSetting()
                sl.add(
                    SingleChoiceSetting(
                        context, InputMappingIntSetting(attachmentSetting),
                        R.string.wiimote_extensions, 0, R.array.wiimoteExtensionsEntries,
                        R.array.wiimoteExtensionsValues,
                        MenuTag.getWiimoteExtensionMenuTag(controllerNumber)
                    )
                )
            }

            val numericSettingCount = group.getNumericSettingCount()
            for (j in 0 until numericSettingCount) {
                addNumericSetting(sl, group.getNumericSetting(j))
            }
        }
    }

    private fun addNumericSetting(sl: ArrayList<SettingsItem>, setting: NumericSetting) {
        when (setting.getType()) {
            NumericSetting.TYPE_DOUBLE -> sl.add(
                FloatSliderSetting(
                    InputMappingDoubleSetting(setting),
                    setting.getUiName(),
                    "",
                    ceil(setting.getDoubleMin()).toFloat(),
                    floor(setting.getDoubleMax()).toFloat(),
                    setting.getUiSuffix(),
                    0.5f,
                    true
                )
            )

            NumericSetting.TYPE_BOOLEAN -> sl.add(
                SwitchSetting(
                    InputMappingBooleanSetting(setting),
                    setting.getUiName(),
                    setting.getUiDescription()
                )
            )
        }
    }

    fun updateOldControllerSettingsWarningVisibility() {
        updateOldControllerSettingsWarningVisibility(menuTag.correspondingEmulatedController)
    }

    private fun updateOldControllerSettingsWarningVisibility(controller: EmulatedController) {
        val defaultDevice = controller.getDefaultDevice()

        hasOldControllerSettings = defaultDevice.startsWith("Android/") &&
                defaultDevice.endsWith("/Touchscreen")

        fragmentView.setOldControllerSettingsWarningVisibility(hasOldControllerSettings)
    }

    private fun loadDefaultControllerSettings(controller: EmulatedController) {
        controller.loadDefaultSettings()
        fragmentView.onControllerSettingsChanged()
    }

    private fun clearControllerSettings(controller: EmulatedController) {
        controller.clearSettings()
        fragmentView.onControllerSettingsChanged()
    }

    fun setAllLogTypes(value: Boolean) {
        val settings = fragmentView.settings

        for ((key) in LOG_TYPE_NAMES) {
            AdHocBooleanSetting(
                Settings.FILE_LOGGER,
                Settings.SECTION_LOGGER_LOGS,
                key,
                false
            ).setBoolean(settings!!, value)
        }

        fragmentView.adapter!!.notifyAllSettingsChanged()
    }

    private fun convertOnThread(f: BooleanSupplier) {
        ThreadUtil.runOnThreadAndShowResult(
            fragmentView.fragmentActivity,
            R.string.wii_converting,
            0,
            { context.resources.getString(if (f.get()) R.string.wii_convert_success else R.string.wii_convert_failure) }
        )
    }

    fun installDriver(uri: Uri) {
        val context = this.context.applicationContext
        CoroutineScope(Dispatchers.IO).launch {
            val stream = context.contentResolver.openInputStream(uri)
            if (stream == null) {
                GpuDriverHelper.uninstallDriver()
                withContext(Dispatchers.Main) {
                    fragmentView.onDriverInstallDone(GpuDriverInstallResult.FileNotFound)
                }
                return@launch
            }

            val result = GpuDriverHelper.installDriver(stream)
            withContext(Dispatchers.Main) {
                with(this@SettingsFragmentPresenter) {
                    this.gpuDriver = GpuDriverHelper.getInstalledDriverMetadata()
                        ?: GpuDriverHelper.getSystemDriverMetadata(context) ?: return@withContext
                    this.libNameSetting.setString(this.settings!!, this.gpuDriver!!.libraryName)
                }
                fragmentView.onDriverInstallDone(result)
            }
        }
    }

    fun useSystemDriver() {
        CoroutineScope(Dispatchers.IO).launch {
            GpuDriverHelper.uninstallDriver()
            withContext(Dispatchers.Main) {
                with(this@SettingsFragmentPresenter) {
                    this.gpuDriver =
                        GpuDriverHelper.getInstalledDriverMetadata()
                            ?: GpuDriverHelper.getSystemDriverMetadata(context.applicationContext)
                    this.libNameSetting.setString(this.settings!!, "")
                }
                fragmentView.onDriverUninstallDone()
            }
        }
    }

    companion object {
        private val LOG_TYPE_NAMES = NativeLibrary.GetLogTypeNames()
        const val ARG_CONTROLLER_TYPE = "controller_type"
        const val ARG_SERIALPORT1_TYPE = "serialport1_type"

        // Value obtained from LogLevel in Common/Logging/Log.h
        private val logVerbosityEntries: Int
            get() =
                if (NativeLibrary.GetMaxLogLevel() == 5) {
                    R.array.logVerbosityEntriesMaxLevelDebug
                } else {
                    R.array.logVerbosityEntriesMaxLevelInfo
                }

        // Value obtained from LogLevel in Common/Logging/Log.h
        private val logVerbosityValues: Int
            get() =
                if (NativeLibrary.GetMaxLogLevel() == 5) {
                    R.array.logVerbosityValuesMaxLevelDebug
                } else {
                    R.array.logVerbosityValuesMaxLevelInfo
                }
    }
}
