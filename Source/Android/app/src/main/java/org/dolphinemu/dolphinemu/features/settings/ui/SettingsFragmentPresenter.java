// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.collection.ArraySet;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.UserDataActivity;
import org.dolphinemu.dolphinemu.features.input.model.InputMappingBooleanSetting;
import org.dolphinemu.dolphinemu.features.input.model.InputMappingDoubleSetting;
import org.dolphinemu.dolphinemu.features.input.model.InputMappingIntSetting;
import org.dolphinemu.dolphinemu.features.input.model.controlleremu.ControlGroup;
import org.dolphinemu.dolphinemu.features.input.model.controlleremu.EmulatedController;
import org.dolphinemu.dolphinemu.features.input.model.controlleremu.NumericSetting;
import org.dolphinemu.dolphinemu.features.input.model.view.InputMappingControlSetting;
import org.dolphinemu.dolphinemu.features.settings.model.AbstractBooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.AbstractIntSetting;
import org.dolphinemu.dolphinemu.features.settings.model.AdHocBooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.FloatSetting;
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting;
import org.dolphinemu.dolphinemu.features.settings.model.PostProcessing;
import org.dolphinemu.dolphinemu.features.settings.model.ScaledIntSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.DateTimeChoiceSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.SwitchSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.FilePicker;
import org.dolphinemu.dolphinemu.features.settings.model.view.FloatSliderSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.HeaderSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.HyperLinkHeaderSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.InputStringSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.IntSliderSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.InvertedSwitchSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.LogSwitchSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.PercentSliderSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.RunRunnable;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.model.view.SingleChoiceSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.SingleChoiceSettingDynamicDescriptions;
import org.dolphinemu.dolphinemu.features.settings.model.view.StringSingleChoiceSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.SubmenuSetting;
import org.dolphinemu.dolphinemu.ui.main.MainPresenter;
import org.dolphinemu.dolphinemu.utils.BooleanSupplier;
import org.dolphinemu.dolphinemu.utils.EGLHelper;
import org.dolphinemu.dolphinemu.utils.ThemeHelper;
import org.dolphinemu.dolphinemu.utils.ThreadUtil;
import org.dolphinemu.dolphinemu.utils.WiiUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Set;

public final class SettingsFragmentPresenter
{
  private final SettingsFragmentView mView;
  private final Context mContext;

  private static final LinkedHashMap<String, String> LOG_TYPE_NAMES =
          NativeLibrary.GetLogTypeNames();

  public static final String ARG_CONTROLLER_TYPE = "controller_type";
  public static final String ARG_SERIALPORT1_TYPE = "serialport1_type";
  private MenuTag mMenuTag;
  private String mGameID;

  private Settings mSettings;
  private ArrayList<SettingsItem> mSettingsList;

  private int mSerialPort1Type;
  private int mControllerNumber;
  private int mControllerType;

  public SettingsFragmentPresenter(SettingsFragmentView view, Context context)
  {
    mView = view;
    mContext = context;
  }

  public void onCreate(MenuTag menuTag, String gameId, Bundle extras)
  {
    mGameID = gameId;
    this.mMenuTag = menuTag;

    if (menuTag.isGCPadMenu() || menuTag.isWiimoteExtensionMenu())
    {
      mControllerNumber = menuTag.getSubType();
      mControllerType = extras.getInt(ARG_CONTROLLER_TYPE);
    }
    else if (menuTag.isWiimoteMenu())
    {
      mControllerNumber = menuTag.getSubType();
    }
    else if (menuTag.isSerialPort1Menu())
    {
      mSerialPort1Type = extras.getInt(ARG_SERIALPORT1_TYPE);
    }
  }

  public void onViewCreated(MenuTag menuTag, Settings settings)
  {
    this.mMenuTag = menuTag;

    if (!TextUtils.isEmpty(mGameID))
    {
      mView.getActivity().setTitle(mContext.getString(R.string.game_settings, mGameID));
    }

    setSettings(settings);
  }

  public Settings getSettings()
  {
    return mSettings;
  }

  public void loadDefaultSettings()
  {
    loadSettingsList();
  }

  public void setSettings(Settings settings)
  {
    if (mSettingsList == null && settings != null)
    {
      mSettings = settings;

      loadSettingsList();
    }
    else
    {
      mView.showSettingsList(mSettingsList);
    }
  }

  private void loadSettingsList()
  {
    ArrayList<SettingsItem> sl = new ArrayList<>();

    switch (mMenuTag)
    {
      case SETTINGS:
        addTopLevelSettings(sl);
        break;

      case CONFIG:
        addConfigSettings(sl);
        break;

      case CONFIG_GENERAL:
        addGeneralSettings(sl);
        break;

      case CONFIG_INTERFACE:
        addInterfaceSettings(sl);
        break;

      case CONFIG_AUDIO:
        addAudioSettings(sl);
        break;

      case CONFIG_PATHS:
        addPathsSettings(sl);
        break;

      case CONFIG_GAME_CUBE:
        addGameCubeSettings(sl);
        break;

      case CONFIG_WII:
        addWiiSettings(sl);
        break;

      case CONFIG_ADVANCED:
        addAdvancedSettings(sl);
        break;

      case GRAPHICS:
        addGraphicsSettings(sl);
        break;

      case CONFIG_SERIALPORT1:
        addSerialPortSubSettings(sl, mSerialPort1Type);
        break;

      case GCPAD_TYPE:
        addGcPadSettings(sl);
        break;

      case WIIMOTE:
        addWiimoteSettings(sl);
        break;

      case ENHANCEMENTS:
        addEnhanceSettings(sl);
        break;

      case STEREOSCOPY:
        addStereoSettings(sl);
        break;

      case HACKS:
        addHackSettings(sl);
        break;

      case STATISTICS:
        addStatisticsSettings(sl);
        break;

      case ADVANCED_GRAPHICS:
        addAdvancedGraphicsSettings(sl);
        break;

      case CONFIG_LOG:
        addLogConfigurationSettings(sl);
        break;

      case DEBUG:
        addDebugSettings(sl);
        break;

      case GCPAD_1:
      case GCPAD_2:
      case GCPAD_3:
      case GCPAD_4:
        addGcPadSubSettings(sl, mControllerNumber, mControllerType);
        break;

      case WIIMOTE_1:
      case WIIMOTE_2:
      case WIIMOTE_3:
      case WIIMOTE_4:
        addWiimoteSubSettings(sl, mControllerNumber);
        break;

      case WIIMOTE_EXTENSION_1:
      case WIIMOTE_EXTENSION_2:
      case WIIMOTE_EXTENSION_3:
      case WIIMOTE_EXTENSION_4:
        addExtensionTypeSettings(sl, mControllerNumber, mControllerType);
        break;

      case WIIMOTE_GENERAL_1:
      case WIIMOTE_GENERAL_2:
      case WIIMOTE_GENERAL_3:
      case WIIMOTE_GENERAL_4:
        addWiimoteGeneralSubSettings(sl, mControllerNumber);
        break;

      case WIIMOTE_MOTION_SIMULATION_1:
      case WIIMOTE_MOTION_SIMULATION_2:
      case WIIMOTE_MOTION_SIMULATION_3:
      case WIIMOTE_MOTION_SIMULATION_4:
        addWiimoteMotionSimulationSubSettings(sl, mControllerNumber);
        break;

      case WIIMOTE_MOTION_INPUT_1:
      case WIIMOTE_MOTION_INPUT_2:
      case WIIMOTE_MOTION_INPUT_3:
      case WIIMOTE_MOTION_INPUT_4:
        addWiimoteMotionInputSubSettings(sl, mControllerNumber);
        break;

      default:
        throw new UnsupportedOperationException("Unimplemented menu");
    }

    mSettingsList = sl;
    mView.showSettingsList(mSettingsList);
  }

  private void addTopLevelSettings(ArrayList<SettingsItem> sl)
  {
    sl.add(new SubmenuSetting(mContext, R.string.config, MenuTag.CONFIG));
    sl.add(new SubmenuSetting(mContext, R.string.graphics_settings, MenuTag.GRAPHICS));

    if (!NativeLibrary.IsRunning())
    {
      sl.add(new SubmenuSetting(mContext, R.string.gcpad_settings, MenuTag.GCPAD_TYPE));
      if (mSettings.isWii())
      {
        sl.add(new SubmenuSetting(mContext, R.string.wiimote_settings, MenuTag.WIIMOTE));
      }
    }

    sl.add(new HeaderSetting(mContext, R.string.setting_clear_info, 0));
  }

  private void addConfigSettings(ArrayList<SettingsItem> sl)
  {
    sl.add(new SubmenuSetting(mContext, R.string.general_submenu, MenuTag.CONFIG_GENERAL));
    sl.add(new SubmenuSetting(mContext, R.string.interface_submenu, MenuTag.CONFIG_INTERFACE));
    sl.add(new SubmenuSetting(mContext, R.string.audio_submenu, MenuTag.CONFIG_AUDIO));
    sl.add(new SubmenuSetting(mContext, R.string.paths_submenu, MenuTag.CONFIG_PATHS));
    sl.add(new SubmenuSetting(mContext, R.string.gamecube_submenu, MenuTag.CONFIG_GAME_CUBE));
    sl.add(new SubmenuSetting(mContext, R.string.wii_submenu, MenuTag.CONFIG_WII));
    sl.add(new SubmenuSetting(mContext, R.string.advanced_submenu, MenuTag.CONFIG_ADVANCED));
    sl.add(new SubmenuSetting(mContext, R.string.log_submenu, MenuTag.CONFIG_LOG));
    sl.add(new SubmenuSetting(mContext, R.string.debug_submenu, MenuTag.DEBUG));
    sl.add(new RunRunnable(mContext, R.string.user_data_submenu, 0, 0, 0, false,
            () -> UserDataActivity.launch(mContext)));
  }

  private void addGeneralSettings(ArrayList<SettingsItem> sl)
  {
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_CPU_THREAD, R.string.dual_core,
            R.string.dual_core_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_ENABLE_CHEATS, R.string.enable_cheats,
            0));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_OVERRIDE_REGION_SETTINGS,
            R.string.override_region_settings, 0));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_AUTO_DISC_CHANGE,
            R.string.auto_disc_change, 0));
    sl.add(new PercentSliderSetting(mContext, FloatSetting.MAIN_EMULATION_SPEED,
            R.string.speed_limit, 0, 0, 200, "%", 1));
    sl.add(new SingleChoiceSetting(mContext, IntSetting.MAIN_FALLBACK_REGION,
            R.string.fallback_region, 0, R.array.regionEntries, R.array.regionValues));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_ANALYTICS_ENABLED, R.string.analytics,
            0));
    sl.add(new RunRunnable(mContext, R.string.analytics_new_id, 0,
            R.string.analytics_new_id_confirmation, 0, true,
            NativeLibrary::GenerateNewStatisticsId));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_ENABLE_SAVESTATES,
            R.string.enable_save_states, R.string.enable_save_states_description));
  }

  private void addInterfaceSettings(ArrayList<SettingsItem> sl)
  {
    // Hide the orientation setting if the device only supports one orientation. Old devices which
    // support both portrait and landscape may report support for neither, so we use ==, not &&.
    PackageManager packageManager = mContext.getPackageManager();
    if (packageManager.hasSystemFeature(PackageManager.FEATURE_SCREEN_PORTRAIT) ==
            packageManager.hasSystemFeature(PackageManager.FEATURE_SCREEN_LANDSCAPE))
    {
      sl.add(new SingleChoiceSetting(mContext, IntSetting.MAIN_EMULATION_ORIENTATION,
              R.string.emulation_screen_orientation, 0, R.array.orientationEntries,
              R.array.orientationValues));
    }

    // Only android 9+ supports this feature.
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P)
    {
      sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_EXPAND_TO_CUTOUT_AREA,
              R.string.expand_to_cutout_area, R.string.expand_to_cutout_area_description));
    }

    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_USE_PANIC_HANDLERS,
            R.string.panic_handlers, R.string.panic_handlers_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_OSD_MESSAGES, R.string.osd_messages,
            R.string.osd_messages_description));

    AbstractIntSetting appTheme = new AbstractIntSetting()
    {
      @Override
      public boolean isOverridden(@NonNull Settings settings)
      {
        return IntSetting.MAIN_INTERFACE_THEME.isOverridden(settings);
      }

      @Override
      public boolean isRuntimeEditable()
      {
        // This only affects app UI
        return true;
      }

      @Override
      public boolean delete(@NonNull Settings settings)
      {
        ThemeHelper.deleteThemeKey((AppCompatActivity) mView.getActivity());
        return IntSetting.MAIN_INTERFACE_THEME.delete(settings);
      }

      @Override
      public int getInt(@NonNull Settings settings)
      {
        return IntSetting.MAIN_INTERFACE_THEME.getInt(settings);
      }

      @Override
      public void setInt(@NonNull Settings settings, int newValue)
      {
        IntSetting.MAIN_INTERFACE_THEME.setInt(settings, newValue);
        ThemeHelper.saveTheme((AppCompatActivity) mView.getActivity(), newValue);
      }
    };

    // If a Monet theme is run on a device below API 31, the app will crash
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
    {
      sl.add(new SingleChoiceSetting(mContext, appTheme, R.string.change_theme, 0,
              R.array.themeEntriesA12, R.array.themeValuesA12));
    }
    else
    {
      sl.add(new SingleChoiceSetting(mContext, appTheme, R.string.change_theme, 0,
              R.array.themeEntries, R.array.themeValues));
    }

    AbstractIntSetting themeMode = new AbstractIntSetting()
    {
      @Override
      public boolean isOverridden(@NonNull Settings settings)
      {
        return IntSetting.MAIN_INTERFACE_THEME_MODE.isOverridden(settings);
      }

      @Override
      public boolean isRuntimeEditable()
      {
        // This only affects app UI
        return true;
      }

      @Override
      public boolean delete(@NonNull Settings settings)
      {
        ThemeHelper.deleteThemeModeKey((AppCompatActivity) mView.getActivity());
        return IntSetting.MAIN_INTERFACE_THEME_MODE.delete(settings);
      }

      @Override
      public int getInt(@NonNull Settings settings)
      {
        return IntSetting.MAIN_INTERFACE_THEME_MODE.getInt(settings);
      }

      @Override
      public void setInt(@NonNull Settings settings, int newValue)
      {
        IntSetting.MAIN_INTERFACE_THEME_MODE.setInt(settings, newValue);
        ThemeHelper.saveThemeMode((AppCompatActivity) mView.getActivity(), newValue);
      }
    };

    sl.add(new SingleChoiceSetting(mContext, themeMode, R.string.change_theme_mode, 0,
            R.array.themeModeEntries, R.array.themeModeValues));

    AbstractBooleanSetting blackBackgrounds = new AbstractBooleanSetting()
    {
      @Override
      public boolean isOverridden(@NonNull Settings settings)
      {
        return BooleanSetting.MAIN_USE_BLACK_BACKGROUNDS.isOverridden(settings);
      }

      @Override
      public boolean isRuntimeEditable()
      {
        return true;
      }

      @Override
      public boolean delete(@NonNull Settings settings)
      {
        ThemeHelper.deleteBackgroundSetting((AppCompatActivity) mView.getActivity());
        return BooleanSetting.MAIN_USE_BLACK_BACKGROUNDS.delete(settings);
      }

      @Override
      public boolean getBoolean(@NonNull Settings settings)
      {
        return BooleanSetting.MAIN_USE_BLACK_BACKGROUNDS.getBoolean(settings);
      }

      @Override
      public void setBoolean(@NonNull Settings settings, boolean newValue)
      {
        BooleanSetting.MAIN_USE_BLACK_BACKGROUNDS.setBoolean(settings, newValue);
        ThemeHelper.saveBackgroundSetting((AppCompatActivity) mView.getActivity(), newValue);
      }
    };

    sl.add(new SwitchSetting(mContext, blackBackgrounds, R.string.use_black_backgrounds,
            R.string.use_black_backgrounds_description));
  }

  private void addAudioSettings(ArrayList<SettingsItem> sl)
  {
    final int DSP_HLE = 0;
    final int DSP_LLE_RECOMPILER = 1;
    final int DSP_LLE_INTERPRETER = 2;

    AbstractIntSetting dspEmulationEngine = new AbstractIntSetting()
    {
      @Override
      public int getInt(@NonNull Settings settings)
      {
        if (BooleanSetting.MAIN_DSP_HLE.getBoolean(settings))
        {
          return DSP_HLE;
        }
        else
        {
          boolean jit = BooleanSetting.MAIN_DSP_JIT.getBoolean(settings);
          return jit ? DSP_LLE_RECOMPILER : DSP_LLE_INTERPRETER;
        }
      }

      @Override
      public void setInt(@NonNull Settings settings, int newValue)
      {
        switch (newValue)
        {
          case DSP_HLE:
            BooleanSetting.MAIN_DSP_HLE.setBoolean(settings, true);
            BooleanSetting.MAIN_DSP_JIT.setBoolean(settings, true);
            break;

          case DSP_LLE_RECOMPILER:
            BooleanSetting.MAIN_DSP_HLE.setBoolean(settings, false);
            BooleanSetting.MAIN_DSP_JIT.setBoolean(settings, true);
            break;

          case DSP_LLE_INTERPRETER:
            BooleanSetting.MAIN_DSP_HLE.setBoolean(settings, false);
            BooleanSetting.MAIN_DSP_JIT.setBoolean(settings, false);
            break;
        }
      }

      @Override
      public boolean isOverridden(@NonNull Settings settings)
      {
        return BooleanSetting.MAIN_DSP_HLE.isOverridden(settings) ||
                BooleanSetting.MAIN_DSP_JIT.isOverridden(settings);
      }

      @Override
      public boolean isRuntimeEditable()
      {
        return BooleanSetting.MAIN_DSP_HLE.isRuntimeEditable() &&
                BooleanSetting.MAIN_DSP_JIT.isRuntimeEditable();
      }

      @Override
      public boolean delete(@NonNull Settings settings)
      {
        // Not short circuiting
        return BooleanSetting.MAIN_DSP_HLE.delete(settings) &
                BooleanSetting.MAIN_DSP_JIT.delete(settings);
      }
    };

    // TODO: Exclude values from arrays instead of having multiple arrays.
    int defaultCpuCore = NativeLibrary.DefaultCPUCore();
    int dspEngineEntries;
    int dspEngineValues;
    if (defaultCpuCore == 1)  // x86-64
    {
      dspEngineEntries = R.array.dspEngineEntriesX86_64;
      dspEngineValues = R.array.dspEngineValuesX86_64;
    }
    else  // Generic
    {
      dspEngineEntries = R.array.dspEngineEntriesGeneric;
      dspEngineValues = R.array.dspEngineValuesGeneric;
    }
    sl.add(new SingleChoiceSetting(mContext, dspEmulationEngine, R.string.dsp_emulation_engine, 0,
            dspEngineEntries, dspEngineValues));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_AUDIO_STRETCH, R.string.audio_stretch,
            R.string.audio_stretch_description));
    sl.add(new IntSliderSetting(mContext, IntSetting.MAIN_AUDIO_VOLUME, R.string.audio_volume, 0,
            0, 100, "%", 1));
  }

  private void addPathsSettings(ArrayList<SettingsItem> sl)
  {
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_RECURSIVE_ISO_PATHS,
            R.string.search_subfolders, 0));
    sl.add(new FilePicker(mContext, StringSetting.MAIN_DEFAULT_ISO, R.string.default_ISO, 0,
            MainPresenter.REQUEST_GAME_FILE, null));
    sl.add(new FilePicker(mContext, StringSetting.MAIN_FS_PATH, R.string.wii_NAND_root, 0,
            MainPresenter.REQUEST_DIRECTORY, "/Wii"));
    sl.add(new FilePicker(mContext, StringSetting.MAIN_DUMP_PATH, R.string.dump_path, 0,
            MainPresenter.REQUEST_DIRECTORY, "/Dump"));
    sl.add(new FilePicker(mContext, StringSetting.MAIN_LOAD_PATH, R.string.load_path, 0,
            MainPresenter.REQUEST_DIRECTORY, "/Load"));
    sl.add(new FilePicker(mContext, StringSetting.MAIN_RESOURCEPACK_PATH,
            R.string.resource_pack_path, 0, MainPresenter.REQUEST_DIRECTORY, "/ResourcePacks"));
    sl.add(new FilePicker(mContext, StringSetting.MAIN_WFS_PATH, R.string.wfs_path, 0,
            MainPresenter.REQUEST_DIRECTORY, "/WFS"));
  }

  private void addGameCubeSettings(ArrayList<SettingsItem> sl)
  {
    sl.add(new HeaderSetting(mContext, R.string.ipl_settings, 0));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_SKIP_IPL, R.string.skip_main_menu,
            R.string.skip_main_menu_description));
    sl.add(new SingleChoiceSetting(mContext, IntSetting.MAIN_GC_LANGUAGE, R.string.system_language,
            0, R.array.gameCubeSystemLanguageEntries, R.array.gameCubeSystemLanguageValues));

    sl.add(new HeaderSetting(mContext, R.string.device_settings, 0));
    sl.add(new SingleChoiceSetting(mContext, IntSetting.MAIN_SLOT_A, R.string.slot_a_device, 0,
            R.array.slotDeviceEntries, R.array.slotDeviceValues));
    sl.add(new SingleChoiceSetting(mContext, IntSetting.MAIN_SLOT_B, R.string.slot_b_device, 0,
            R.array.slotDeviceEntries, R.array.slotDeviceValues));
    sl.add(new SingleChoiceSetting(mContext, IntSetting.MAIN_SERIAL_PORT_1,
            R.string.serial_port_1_device, 0,
            R.array.serialPort1DeviceEntries, R.array.serialPort1DeviceValues,
            MenuTag.CONFIG_SERIALPORT1));
  }

  private void addWiiSettings(ArrayList<SettingsItem> sl)
  {
    sl.add(new HeaderSetting(mContext, R.string.wii_misc_settings, 0));
    sl.add(new SingleChoiceSetting(mContext, IntSetting.SYSCONF_LANGUAGE, R.string.system_language,
            0, R.array.wiiSystemLanguageEntries, R.array.wiiSystemLanguageValues));
    sl.add(new SwitchSetting(mContext, BooleanSetting.SYSCONF_WIDESCREEN, R.string.wii_widescreen,
            R.string.wii_widescreen_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.SYSCONF_PAL60, R.string.wii_pal60,
            R.string.wii_pal60_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.SYSCONF_SCREENSAVER,
            R.string.wii_screensaver, R.string.wii_screensaver_description));
    sl.add(new SingleChoiceSetting(mContext, IntSetting.SYSCONF_SOUND_MODE, R.string.sound_mode, 0,
            R.array.soundModeEntries, R.array.soundModeValues));

    sl.add(new HeaderSetting(mContext, R.string.wii_sd_card_settings, 0));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_WII_SD_CARD, R.string.insert_sd_card,
            R.string.insert_sd_card_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_ALLOW_SD_WRITES,
            R.string.wii_sd_card_allow_writes, 0));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_WII_SD_CARD_ENABLE_FOLDER_SYNC,
            R.string.wii_sd_card_sync, R.string.wii_sd_card_sync_description));
    // TODO: Hardcoding "Load" here is wrong, because the user may have changed the Load path.
    // The code structure makes this hard to fix, and with scoped storage active the Load path
    // can't be changed anyway
    sl.add(new FilePicker(mContext, StringSetting.MAIN_WII_SD_CARD_IMAGE_PATH,
            R.string.wii_sd_card_path, 0, MainPresenter.REQUEST_SD_FILE, "/Load/WiiSD.raw"));
    sl.add(new FilePicker(mContext, StringSetting.MAIN_WII_SD_CARD_SYNC_FOLDER_PATH,
            R.string.wii_sd_sync_folder, 0, MainPresenter.REQUEST_DIRECTORY, "/Load/WiiSDSync/"));
    sl.add(new RunRunnable(mContext, R.string.wii_sd_card_folder_to_file, 0,
            R.string.wii_sd_card_folder_to_file_confirmation, 0, false,
            () -> convertOnThread(WiiUtils::syncSdFolderToSdImage)));
    sl.add(new RunRunnable(mContext, R.string.wii_sd_card_file_to_folder, 0,
            R.string.wii_sd_card_file_to_folder_confirmation, 0, false,
            () -> convertOnThread(WiiUtils::syncSdImageToSdFolder)));

    sl.add(new HeaderSetting(mContext, R.string.wii_wiimote_settings, 0));
    sl.add(new SwitchSetting(mContext, BooleanSetting.SYSCONF_WIIMOTE_MOTOR,
            R.string.wiimote_rumble, 0));
    sl.add(new IntSliderSetting(mContext, IntSetting.SYSCONF_SPEAKER_VOLUME,
            R.string.wiimote_volume, 0, 0, 127, "", 1));
    sl.add(new IntSliderSetting(mContext, IntSetting.SYSCONF_SENSOR_BAR_SENSITIVITY,
            R.string.sensor_bar_sensitivity, 0, 1, 5, "", 1));
    sl.add(new SingleChoiceSetting(mContext, IntSetting.SYSCONF_SENSOR_BAR_POSITION,
            R.string.sensor_bar_position, 0, R.array.sensorBarPositionEntries,
            R.array.sensorBarPositionValues));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_WIIMOTE_CONTINUOUS_SCANNING,
            R.string.wiimote_scanning, R.string.wiimote_scanning_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_WIIMOTE_ENABLE_SPEAKER,
            R.string.wiimote_speaker, R.string.wiimote_speaker_description));

    sl.add(new HeaderSetting(mContext, R.string.emulated_usb_devices, 0));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_EMULATE_SKYLANDER_PORTAL,
            R.string.emulate_skylander_portal, 0));
  }

  private void addAdvancedSettings(ArrayList<SettingsItem> sl)
  {
    final int SYNC_GPU_NEVER = 0;
    final int SYNC_GPU_ON_IDLE_SKIP = 1;
    final int SYNC_GPU_ALWAYS = 2;

    AbstractIntSetting synchronizeGpuThread = new AbstractIntSetting()
    {
      @Override
      public int getInt(@NonNull Settings settings)
      {
        if (BooleanSetting.MAIN_SYNC_GPU.getBoolean(settings))
        {
          return SYNC_GPU_ALWAYS;
        }
        else
        {
          boolean syncOnSkipIdle = BooleanSetting.MAIN_SYNC_ON_SKIP_IDLE.getBoolean(settings);
          return syncOnSkipIdle ? SYNC_GPU_ON_IDLE_SKIP : SYNC_GPU_NEVER;
        }
      }

      @Override
      public void setInt(@NonNull Settings settings, int newValue)
      {
        switch (newValue)
        {
          case SYNC_GPU_NEVER:
            BooleanSetting.MAIN_SYNC_ON_SKIP_IDLE.setBoolean(settings, false);
            BooleanSetting.MAIN_SYNC_GPU.setBoolean(settings, false);
            break;

          case SYNC_GPU_ON_IDLE_SKIP:
            BooleanSetting.MAIN_SYNC_ON_SKIP_IDLE.setBoolean(settings, true);
            BooleanSetting.MAIN_SYNC_GPU.setBoolean(settings, false);
            break;

          case SYNC_GPU_ALWAYS:
            BooleanSetting.MAIN_SYNC_ON_SKIP_IDLE.setBoolean(settings, true);
            BooleanSetting.MAIN_SYNC_GPU.setBoolean(settings, true);
            break;
        }
      }

      @Override
      public boolean isOverridden(@NonNull Settings settings)
      {
        return BooleanSetting.MAIN_SYNC_ON_SKIP_IDLE.isOverridden(settings) ||
                BooleanSetting.MAIN_SYNC_GPU.isOverridden(settings);
      }

      @Override
      public boolean isRuntimeEditable()
      {
        return BooleanSetting.MAIN_SYNC_ON_SKIP_IDLE.isRuntimeEditable() &&
                BooleanSetting.MAIN_SYNC_GPU.isRuntimeEditable();
      }

      @Override
      public boolean delete(@NonNull Settings settings)
      {
        // Not short circuiting
        return BooleanSetting.MAIN_SYNC_ON_SKIP_IDLE.delete(settings) &
                BooleanSetting.MAIN_SYNC_GPU.delete(settings);
      }
    };

    // TODO: Having different emuCoresEntries/emuCoresValues for each architecture is annoying.
    //       The proper solution would be to have one set of entries and one set of values
    //       and exclude the values that aren't present in PowerPC::AvailableCPUCores().
    int defaultCpuCore = NativeLibrary.DefaultCPUCore();
    int emuCoresEntries;
    int emuCoresValues;
    if (defaultCpuCore == 1)  // x86-64
    {
      emuCoresEntries = R.array.emuCoresEntriesX86_64;
      emuCoresValues = R.array.emuCoresValuesX86_64;
    }
    else if (defaultCpuCore == 4)  // AArch64
    {
      emuCoresEntries = R.array.emuCoresEntriesARM64;
      emuCoresValues = R.array.emuCoresValuesARM64;
    }
    else
    {
      emuCoresEntries = R.array.emuCoresEntriesGeneric;
      emuCoresValues = R.array.emuCoresValuesGeneric;
    }
    sl.add(new HeaderSetting(mContext, R.string.cpu_options, 0));
    sl.add(new SingleChoiceSetting(mContext, IntSetting.MAIN_CPU_CORE, R.string.cpu_core, 0,
            emuCoresEntries, emuCoresValues));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_MMU, R.string.mmu_enable,
            R.string.mmu_enable_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_PAUSE_ON_PANIC, R.string.pause_on_panic,
            R.string.pause_on_panic_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_ACCURATE_CPU_CACHE,
            R.string.enable_cpu_cache, R.string.enable_cpu_cache_description));

    sl.add(new HeaderSetting(mContext, R.string.clock_override, 0));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_OVERCLOCK_ENABLE,
            R.string.overclock_enable, R.string.overclock_enable_description));
    sl.add(new PercentSliderSetting(mContext, FloatSetting.MAIN_OVERCLOCK, R.string.overclock_title,
            R.string.overclock_title_description, 0, 400, "%", 1));

    ScaledIntSetting mem1Size = new ScaledIntSetting(1024 * 1024, IntSetting.MAIN_MEM1_SIZE);
    ScaledIntSetting mem2Size = new ScaledIntSetting(1024 * 1024, IntSetting.MAIN_MEM2_SIZE);

    sl.add(new HeaderSetting(mContext, R.string.memory_override, 0));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_RAM_OVERRIDE_ENABLE,
            R.string.enable_memory_size_override,
            R.string.enable_memory_size_override_description));
    sl.add(new IntSliderSetting(mContext, mem1Size, R.string.main_mem1_size, 0, 24, 64, "MB", 1));
    sl.add(new IntSliderSetting(mContext, mem2Size, R.string.main_mem2_size, 0, 64, 128, "MB", 1));

    sl.add(new HeaderSetting(mContext, R.string.gpu_options, 0));
    sl.add(new SingleChoiceSetting(mContext, synchronizeGpuThread, R.string.synchronize_gpu_thread,
            R.string.synchronize_gpu_thread_description, R.array.synchronizeGpuThreadEntries,
            R.array.synchronizeGpuThreadValues));

    sl.add(new HeaderSetting(mContext, R.string.custom_rtc_options, 0));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_CUSTOM_RTC_ENABLE,
            R.string.custom_rtc_enable, R.string.custom_rtc_description));
    sl.add(new DateTimeChoiceSetting(mContext, StringSetting.MAIN_CUSTOM_RTC_VALUE,
            R.string.set_custom_rtc, 0));

    sl.add(new HeaderSetting(mContext, R.string.misc_settings, 0));
    sl.add(new InvertedSwitchSetting(mContext, BooleanSetting.MAIN_FAST_DISC_SPEED,
            R.string.emulate_disc_speed,
            R.string.emulate_disc_speed_description));
  }

  private void addSerialPortSubSettings(ArrayList<SettingsItem> sl, int serialPort1Type)
  {
    if (serialPort1Type == 10) // Broadband Adapter (XLink Kai)
    {
      sl.add(new HyperLinkHeaderSetting(mContext, R.string.xlink_kai_guide_header, 0));
      sl.add(new InputStringSetting(mContext, StringSetting.MAIN_BBA_XLINK_IP,
              R.string.xlink_kai_bba_ip, R.string.xlink_kai_bba_ip_description));
    }
    else if (serialPort1Type == 12) // Broadband Adapter (Built In)
    {
      sl.add(new InputStringSetting(mContext, StringSetting.MAIN_BBA_BUILTIN_DNS,
              R.string.bba_builtin_dns, R.string.bba_builtin_dns_description));
    }
  }

  private void addGcPadSettings(ArrayList<SettingsItem> sl)
  {
    sl.add(new SingleChoiceSetting(mContext, IntSetting.MAIN_SI_DEVICE_0, R.string.controller_0, 0,
            R.array.gcpadTypeEntries, R.array.gcpadTypeValues, MenuTag.getGCPadMenuTag(0)));
    sl.add(new SingleChoiceSetting(mContext, IntSetting.MAIN_SI_DEVICE_1, R.string.controller_1, 0,
            R.array.gcpadTypeEntries, R.array.gcpadTypeValues, MenuTag.getGCPadMenuTag(1)));
    sl.add(new SingleChoiceSetting(mContext, IntSetting.MAIN_SI_DEVICE_2, R.string.controller_2, 0,
            R.array.gcpadTypeEntries, R.array.gcpadTypeValues, MenuTag.getGCPadMenuTag(2)));
    sl.add(new SingleChoiceSetting(mContext, IntSetting.MAIN_SI_DEVICE_3, R.string.controller_3, 0,
            R.array.gcpadTypeEntries, R.array.gcpadTypeValues, MenuTag.getGCPadMenuTag(3)));
  }

  private void addWiimoteSettings(ArrayList<SettingsItem> sl)
  {
    sl.add(new SingleChoiceSetting(mContext, IntSetting.WIIMOTE_1_SOURCE, R.string.wiimote_0, 0,
            R.array.wiimoteTypeEntries, R.array.wiimoteTypeValues, MenuTag.getWiimoteMenuTag(0)));
    sl.add(new SingleChoiceSetting(mContext, IntSetting.WIIMOTE_2_SOURCE, R.string.wiimote_1, 0,
            R.array.wiimoteTypeEntries, R.array.wiimoteTypeValues, MenuTag.getWiimoteMenuTag(1)));
    sl.add(new SingleChoiceSetting(mContext, IntSetting.WIIMOTE_3_SOURCE, R.string.wiimote_2, 0,
            R.array.wiimoteTypeEntries, R.array.wiimoteTypeValues, MenuTag.getWiimoteMenuTag(2)));
    sl.add(new SingleChoiceSetting(mContext, IntSetting.WIIMOTE_4_SOURCE, R.string.wiimote_3, 0,
            R.array.wiimoteTypeEntries, R.array.wiimoteTypeValues, MenuTag.getWiimoteMenuTag(3)));
  }

  private void addGraphicsSettings(ArrayList<SettingsItem> sl)
  {
    sl.add(new HeaderSetting(mContext, R.string.graphics_general, 0));
    sl.add(new StringSingleChoiceSetting(mContext, StringSetting.MAIN_GFX_BACKEND,
            R.string.video_backend, 0, R.array.videoBackendEntries, R.array.videoBackendValues));
    sl.add(new SingleChoiceSettingDynamicDescriptions(mContext,
            IntSetting.GFX_SHADER_COMPILATION_MODE, R.string.shader_compilation_mode, 0,
            R.array.shaderCompilationModeEntries, R.array.shaderCompilationModeValues,
            R.array.shaderCompilationDescriptionEntries,
            R.array.shaderCompilationDescriptionValues));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_WAIT_FOR_SHADERS_BEFORE_STARTING,
            R.string.wait_for_shaders, R.string.wait_for_shaders_description));
    sl.add(new SingleChoiceSetting(mContext, IntSetting.GFX_ASPECT_RATIO, R.string.aspect_ratio, 0,
            R.array.aspectRatioEntries, R.array.aspectRatioValues));

    sl.add(new HeaderSetting(mContext, R.string.graphics_more_settings, 0));
    sl.add(new SubmenuSetting(mContext, R.string.enhancements_submenu, MenuTag.ENHANCEMENTS));
    sl.add(new SubmenuSetting(mContext, R.string.hacks_submenu, MenuTag.HACKS));
    sl.add(new SubmenuSetting(mContext, R.string.statistics_submenu, MenuTag.STATISTICS));
    sl.add(new SubmenuSetting(mContext, R.string.advanced_graphics_submenu,
            MenuTag.ADVANCED_GRAPHICS));
  }

  private void addEnhanceSettings(ArrayList<SettingsItem> sl)
  {
    sl.add(new SingleChoiceSetting(mContext, IntSetting.GFX_EFB_SCALE, R.string.internal_resolution,
            R.string.internal_resolution_description, R.array.internalResolutionEntries,
            R.array.internalResolutionValues));
    sl.add(new SingleChoiceSetting(mContext, IntSetting.GFX_MSAA, R.string.FSAA,
            R.string.FSAA_description, R.array.FSAAEntries, R.array.FSAAValues));
    sl.add(new SingleChoiceSetting(mContext, IntSetting.GFX_ENHANCE_MAX_ANISOTROPY,
            R.string.anisotropic_filtering, R.string.anisotropic_filtering_description,
            R.array.anisotropicFilteringEntries, R.array.anisotropicFilteringValues));
    sl.add(new SingleChoiceSetting(mContext, IntSetting.GFX_ENHANCE_FORCE_TEXTURE_FILTERING,
            R.string.texture_filtering, R.string.texture_filtering_description,
            R.array.textureFilteringEntries, R.array.textureFilteringValues));

    int stereoModeValue = IntSetting.GFX_STEREO_MODE.getInt(mSettings);
    final int anaglyphMode = 3;
    String[] shaderList = stereoModeValue == anaglyphMode ?
            PostProcessing.getAnaglyphShaderList() : PostProcessing.getShaderList();

    String[] shaderListEntries = new String[shaderList.length + 1];
    shaderListEntries[0] = mContext.getString(R.string.off);
    System.arraycopy(shaderList, 0, shaderListEntries, 1, shaderList.length);

    String[] shaderListValues = new String[shaderList.length + 1];
    shaderListValues[0] = "";
    System.arraycopy(shaderList, 0, shaderListValues, 1, shaderList.length);

    sl.add(new StringSingleChoiceSetting(mContext, StringSetting.GFX_ENHANCE_POST_SHADER,
            R.string.post_processing_shader, 0, shaderListEntries, shaderListValues));

    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_HACK_COPY_EFB_SCALED,
            R.string.scaled_efb_copy, R.string.scaled_efb_copy_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_ENABLE_PIXEL_LIGHTING,
            R.string.per_pixel_lighting, R.string.per_pixel_lighting_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_ENHANCE_FORCE_TRUE_COLOR,
            R.string.force_24bit_color, R.string.force_24bit_color_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_DISABLE_FOG, R.string.disable_fog,
            R.string.disable_fog_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_ENHANCE_DISABLE_COPY_FILTER,
            R.string.disable_copy_filter, R.string.disable_copy_filter_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION,
            R.string.arbitrary_mipmap_detection, R.string.arbitrary_mipmap_detection_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_WIDESCREEN_HACK,
            R.string.wide_screen_hack, R.string.wide_screen_hack_description));

    // Check if we support stereo
    // If we support desktop GL then we must support at least OpenGL 3.2
    // If we only support OpenGLES then we need both OpenGLES 3.1 and AEP
    EGLHelper helper = new EGLHelper(EGLHelper.EGL_OPENGL_ES2_BIT);

    if ((helper.supportsOpenGL() && helper.GetVersion() >= 320) ||
            (helper.supportsGLES3() && helper.GetVersion() >= 310 &&
                    helper.SupportsExtension("GL_ANDROID_extension_pack_es31a")))
    {
      sl.add(new SubmenuSetting(mContext, R.string.stereoscopy_submenu, MenuTag.STEREOSCOPY));
    }
  }

  private void addHackSettings(ArrayList<SettingsItem> sl)
  {
    sl.add(new HeaderSetting(mContext, R.string.embedded_frame_buffer, 0));
    sl.add(new InvertedSwitchSetting(mContext, BooleanSetting.GFX_HACK_EFB_ACCESS_ENABLE,
            R.string.skip_efb_access, R.string.skip_efb_access_description));
    sl.add(new InvertedSwitchSetting(mContext, BooleanSetting.GFX_HACK_EFB_EMULATE_FORMAT_CHANGES,
            R.string.ignore_format_changes, R.string.ignore_format_changes_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_HACK_SKIP_EFB_COPY_TO_RAM,
            R.string.efb_copy_method, R.string.efb_copy_method_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_HACK_DEFER_EFB_COPIES,
            R.string.defer_efb_copies, R.string.defer_efb_copies_description));

    sl.add(new HeaderSetting(mContext, R.string.texture_cache, 0));
    sl.add(new SingleChoiceSetting(mContext, IntSetting.GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES,
            R.string.texture_cache_accuracy, R.string.texture_cache_accuracy_description,
            R.array.textureCacheAccuracyEntries, R.array.textureCacheAccuracyValues));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_ENABLE_GPU_TEXTURE_DECODING,
            R.string.gpu_texture_decoding, R.string.gpu_texture_decoding_description));

    sl.add(new HeaderSetting(mContext, R.string.external_frame_buffer, 0));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_HACK_SKIP_XFB_COPY_TO_RAM,
            R.string.xfb_copy_method, R.string.xfb_copy_method_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_HACK_IMMEDIATE_XFB,
            R.string.immediate_xfb, R.string.immediate_xfb_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_HACK_SKIP_DUPLICATE_XFBS,
            R.string.skip_duplicate_xfbs, R.string.skip_duplicate_xfbs_description));

    sl.add(new HeaderSetting(mContext, R.string.other, 0));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_FAST_DEPTH_CALC,
            R.string.fast_depth_calculation, R.string.fast_depth_calculation_description));
    sl.add(new InvertedSwitchSetting(mContext, BooleanSetting.GFX_HACK_BBOX_ENABLE,
            R.string.disable_bbox, R.string.disable_bbox_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_HACK_VERTEX_ROUNDING,
            R.string.vertex_rounding, R.string.vertex_rounding_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_HACK_VI_SKIP, R.string.vi_skip,
            R.string.vi_skip_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_SAVE_TEXTURE_CACHE_TO_STATE,
            R.string.texture_cache_to_state, R.string.texture_cache_to_state_description));
  }

  private void addStatisticsSettings(ArrayList<SettingsItem> sl)
  {
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_SHOW_FPS, R.string.show_fps,
            R.string.show_fps_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_SHOW_FTIMES, R.string.show_ftimes,
            R.string.show_ftimes_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_SHOW_VPS, R.string.show_vps,
            R.string.show_vps_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_SHOW_VTIMES, R.string.show_vtimes,
            R.string.show_vtimes_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_SHOW_GRAPHS, R.string.show_graphs,
            R.string.show_graphs_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_SHOW_SPEED, R.string.show_speed,
            R.string.show_speed_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_SHOW_SPEED_COLORS,
            R.string.show_speed_colors,
            R.string.show_speed_colors_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_LOG_RENDER_TIME_TO_FILE,
            R.string.log_render_time_to_file,
            R.string.log_render_time_to_file_description));
    sl.add(new IntSliderSetting(mContext, IntSetting.GFX_PERF_SAMP_WINDOW,
            R.string.performance_sample_window, R.string.performance_sample_window_description, 0,
            10000, "ms", 100));
  }

  private void addAdvancedGraphicsSettings(ArrayList<SettingsItem> sl)
  {
    sl.add(new HeaderSetting(mContext, R.string.gfx_mods_and_custom_textures, 0));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_MODS_ENABLE,
            R.string.gfx_mods, R.string.gfx_mods_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_HIRES_TEXTURES,
            R.string.load_custom_texture, R.string.load_custom_texture_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_CACHE_HIRES_TEXTURES,
            R.string.cache_custom_texture, R.string.cache_custom_texture_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_DUMP_TEXTURES,
            R.string.dump_texture, R.string.dump_texture_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_DUMP_BASE_TEXTURES,
            R.string.dump_base_texture, R.string.dump_base_texture_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_DUMP_MIP_TEXTURES,
            R.string.dump_mip_texture, R.string.dump_mip_texture_description));

    sl.add(new HeaderSetting(mContext, R.string.misc, 0));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_CROP, R.string.crop,
            R.string.crop_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.SYSCONF_PROGRESSIVE_SCAN,
            R.string.progressive_scan, 0));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_BACKEND_MULTITHREADING,
            R.string.backend_multithreading, R.string.backend_multithreading_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_PREFER_VS_FOR_LINE_POINT_EXPANSION,
            R.string.prefer_vs_for_point_line_expansion,
            R.string.prefer_vs_for_point_line_expansion_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_CPU_CULL, R.string.cpu_cull,
            R.string.cpu_cull_description));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_HACK_EFB_DEFER_INVALIDATION,
            R.string.defer_efb_invalidation, R.string.defer_efb_invalidation_description));
    sl.add(new InvertedSwitchSetting(mContext, BooleanSetting.GFX_HACK_FAST_TEXTURE_SAMPLING,
            R.string.manual_texture_sampling, R.string.manual_texture_sampling_description));

    sl.add(new HeaderSetting(mContext, R.string.frame_dumping, 0));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_INTERNAL_RESOLUTION_FRAME_DUMPS,
            R.string.internal_resolution_dumps, R.string.internal_resolution_dumps_description));
    sl.add(new IntSliderSetting(mContext, IntSetting.GFX_PNG_COMPRESSION_LEVEL,
            R.string.png_compression_level, 0, 0, 9, "", 1));

    sl.add(new HeaderSetting(mContext, R.string.debugging, 0));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_ENABLE_WIREFRAME,
            R.string.wireframe, R.string.leave_this_unchecked));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_OVERLAY_STATS,
            R.string.show_stats, R.string.leave_this_unchecked));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_TEXFMT_OVERLAY_ENABLE,
            R.string.texture_format, R.string.leave_this_unchecked));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_ENABLE_VALIDATION_LAYER,
            R.string.validation_layer, R.string.leave_this_unchecked));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_DUMP_EFB_TARGET,
            R.string.dump_efb, R.string.leave_this_unchecked));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_DUMP_XFB_TARGET,
            R.string.dump_xfb, R.string.leave_this_unchecked));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_HACK_DISABLE_COPY_TO_VRAM,
            R.string.disable_vram_copies, R.string.leave_this_unchecked));
  }

  private void addLogConfigurationSettings(ArrayList<SettingsItem> sl)
  {
    sl.add(new SwitchSetting(mContext, BooleanSetting.LOGGER_WRITE_TO_FILE, R.string.log_to_file,
            R.string.log_to_file_description));
    sl.add(new SingleChoiceSetting(mContext, IntSetting.LOGGER_VERBOSITY, R.string.log_verbosity, 0,
            getLogVerbosityEntries(), getLogVerbosityValues()));
    sl.add(new RunRunnable(mContext, R.string.log_enable_all, 0,
            R.string.log_enable_all_confirmation, 0, true, () -> setAllLogTypes(true)));
    sl.add(new RunRunnable(mContext, R.string.log_disable_all, 0,
            R.string.log_disable_all_confirmation, 0, true, () -> setAllLogTypes(false)));
    sl.add(new RunRunnable(mContext, R.string.log_clear, 0, R.string.log_clear_confirmation, 0,
            true, SettingsAdapter::clearLog));

    sl.add(new HeaderSetting(mContext, R.string.log_types, 0));
    for (Map.Entry<String, String> entry : LOG_TYPE_NAMES.entrySet())
    {
      sl.add(new LogSwitchSetting(entry.getKey(), entry.getValue(), ""));
    }
  }

  private void addDebugSettings(ArrayList<SettingsItem> sl)
  {
    sl.add(new HeaderSetting(mContext, R.string.debug_warning, 0));
    sl.add(new InvertedSwitchSetting(mContext, BooleanSetting.MAIN_FASTMEM,
            R.string.debug_fastmem, 0));

    sl.add(new HeaderSetting(mContext, R.string.debug_jit_header, 0));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_DEBUG_JIT_OFF, R.string.debug_jitoff,
            0));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_DEBUG_JIT_LOAD_STORE_OFF,
            R.string.debug_jitloadstoreoff, 0));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_DEBUG_JIT_LOAD_STORE_FLOATING_OFF,
            R.string.debug_jitloadstorefloatingoff, 0));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_DEBUG_JIT_LOAD_STORE_PAIRED_OFF,
            R.string.debug_jitloadstorepairedoff, 0));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_DEBUG_JIT_FLOATING_POINT_OFF,
            R.string.debug_jitfloatingpointoff, 0));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_DEBUG_JIT_INTEGER_OFF,
            R.string.debug_jitintegeroff, 0));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_DEBUG_JIT_PAIRED_OFF,
            R.string.debug_jitpairedoff, 0));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_DEBUG_JIT_SYSTEM_REGISTERS_OFF,
            R.string.debug_jitsystemregistersoff, 0));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_DEBUG_JIT_BRANCH_OFF,
            R.string.debug_jitbranchoff, 0));
    sl.add(new SwitchSetting(mContext, BooleanSetting.MAIN_DEBUG_JIT_REGISTER_CACHE_OFF,
            R.string.debug_jitregistercacheoff, 0));
  }

  private void addStereoSettings(ArrayList<SettingsItem> sl)
  {
    sl.add(new SingleChoiceSetting(mContext, IntSetting.GFX_STEREO_MODE, R.string.stereoscopy_mode,
            0, R.array.stereoscopyEntries, R.array.stereoscopyValues));
    sl.add(new IntSliderSetting(mContext, IntSetting.GFX_STEREO_DEPTH, R.string.stereoscopy_depth,
            R.string.stereoscopy_depth_description, 0, 100, "%", 1));
    sl.add(new IntSliderSetting(mContext, IntSetting.GFX_STEREO_CONVERGENCE_PERCENTAGE,
            R.string.stereoscopy_convergence, R.string.stereoscopy_convergence_description, 0, 200,
            "%", 1));
    sl.add(new SwitchSetting(mContext, BooleanSetting.GFX_STEREO_SWAP_EYES,
            R.string.stereoscopy_swap_eyes, R.string.stereoscopy_swap_eyes_description));
  }

  private void addGcPadSubSettings(ArrayList<SettingsItem> sl, int gcPadNumber, int gcPadType)
  {
    if (gcPadType == 6) // Emulated
    {
      addControllerSettings(sl, EmulatedController.getGcPad(gcPadNumber), null);
    }
    else if (gcPadType == 12) // Adapter
    {
      sl.add(new SwitchSetting(mContext, BooleanSetting.getSettingForAdapterRumble(gcPadNumber),
              R.string.gc_adapter_rumble, R.string.gc_adapter_rumble_description));
      sl.add(new SwitchSetting(mContext, BooleanSetting.getSettingForSimulateKonga(gcPadNumber),
              R.string.gc_adapter_bongos, R.string.gc_adapter_bongos_description));
    }
  }

  private void addWiimoteSubSettings(ArrayList<SettingsItem> sl, int wiimoteNumber)
  {
    sl.add(new HeaderSetting(mContext, R.string.wiimote, 0));
    sl.add(new SubmenuSetting(mContext, R.string.wiimote_general,
            MenuTag.getWiimoteGeneralMenuTag(wiimoteNumber)));
    sl.add(new SubmenuSetting(mContext, R.string.wiimote_motion_simulation,
            MenuTag.getWiimoteMotionSimulationMenuTag(wiimoteNumber)));
    sl.add(new SubmenuSetting(mContext, R.string.wiimote_motion_input,
            MenuTag.getWiimoteMotionInputMenuTag(wiimoteNumber)));

    // TYPE_OTHER is included here instead of in addWiimoteGeneralSubSettings so that touchscreen
    // users won't have to dig into a submenu to find the Sideways Wii Remote setting
    addControllerSettings(sl, EmulatedController.getWiimote(wiimoteNumber),
            new ArraySet<>(Arrays.asList(ControlGroup.TYPE_ATTACHMENTS, ControlGroup.TYPE_OTHER)));
  }

  private void addExtensionTypeSettings(ArrayList<SettingsItem> sl, int wiimoteNumber,
          int extensionType)
  {
    addControllerSettings(sl,
            EmulatedController.getWiimoteAttachment(wiimoteNumber, extensionType), null);
  }

  private void addWiimoteGeneralSubSettings(ArrayList<SettingsItem> sl, int wiimoteNumber)
  {
    addControllerSettings(sl, EmulatedController.getWiimote(wiimoteNumber),
            Collections.singleton(ControlGroup.TYPE_BUTTONS));
  }

  private void addWiimoteMotionSimulationSubSettings(ArrayList<SettingsItem> sl, int wiimoteNumber)
  {
    addControllerSettings(sl, EmulatedController.getWiimote(wiimoteNumber),
            new ArraySet<>(Arrays.asList(ControlGroup.TYPE_FORCE, ControlGroup.TYPE_TILT,
                    ControlGroup.TYPE_CURSOR, ControlGroup.TYPE_SHAKE)));
  }

  private void addWiimoteMotionInputSubSettings(ArrayList<SettingsItem> sl, int wiimoteNumber)
  {
    addControllerSettings(sl, EmulatedController.getWiimote(wiimoteNumber),
            new ArraySet<>(Arrays.asList(ControlGroup.TYPE_IMU_ACCELEROMETER,
                    ControlGroup.TYPE_IMU_GYROSCOPE, ControlGroup.TYPE_IMU_CURSOR)));
  }

  /**
   * @param sl The list to place controller settings into.
   * @param controller The controller to add settings for.
   * @param groupTypeFilter If this is non-null, only groups whose types match this are considered.
   */
  private void addControllerSettings(ArrayList<SettingsItem> sl, EmulatedController controller,
          Set<Integer> groupTypeFilter)
  {
    int groupCount = controller.getGroupCount();
    for (int i = 0; i < groupCount; i++)
    {
      ControlGroup group = controller.getGroup(i);
      int groupType = group.getGroupType();
      if (groupTypeFilter != null && !groupTypeFilter.contains(groupType))
        continue;

      sl.add(new HeaderSetting(group.getUiName(), ""));

      int controlCount = group.getControlCount();
      for (int j = 0; j < controlCount; j++)
      {
        sl.add(new InputMappingControlSetting(group.getControl(j), controller));
      }

      if (groupType == ControlGroup.TYPE_ATTACHMENTS)
      {
        NumericSetting attachmentSetting = group.getAttachmentSetting();
        sl.add(new SingleChoiceSetting(mContext, new InputMappingIntSetting(attachmentSetting),
                R.string.wiimote_extensions, 0, R.array.wiimoteExtensionsEntries,
                R.array.wiimoteExtensionsValues,
                MenuTag.getWiimoteExtensionMenuTag(mControllerNumber)));
      }

      int numericSettingCount = group.getNumericSettingCount();
      for (int j = 0; j < numericSettingCount; j++)
      {
        addNumericSetting(sl, group.getNumericSetting(j));
      }
    }
  }

  private void addNumericSetting(ArrayList<SettingsItem> sl, NumericSetting setting)
  {
    switch (setting.getType())
    {
      case NumericSetting.TYPE_DOUBLE:
        sl.add(new FloatSliderSetting(new InputMappingDoubleSetting(setting), setting.getUiName(),
                "", (int) Math.ceil(setting.getDoubleMin()),
                (int) Math.floor(setting.getDoubleMax()), setting.getUiSuffix()));
        break;
      case NumericSetting.TYPE_BOOLEAN:
        sl.add(new SwitchSetting(new InputMappingBooleanSetting(setting), setting.getUiName(),
                setting.getUiDescription()));
        break;
    }
  }

  private static int getLogVerbosityEntries()
  {
    // Value obtained from LogLevel in Common/Logging/Log.h
    if (NativeLibrary.GetMaxLogLevel() == 5)
    {
      return R.array.logVerbosityEntriesMaxLevelDebug;
    }
    else
    {
      return R.array.logVerbosityEntriesMaxLevelInfo;
    }
  }

  private static int getLogVerbosityValues()
  {
    // Value obtained from LogLevel in Common/Logging/Log.h
    if (NativeLibrary.GetMaxLogLevel() == 5)
    {
      return R.array.logVerbosityValuesMaxLevelDebug;
    }
    else
    {
      return R.array.logVerbosityValuesMaxLevelInfo;
    }
  }

  public void setAllLogTypes(boolean value)
  {
    Settings settings = mView.getSettings();

    for (Map.Entry<String, String> entry : LOG_TYPE_NAMES.entrySet())
    {
      new AdHocBooleanSetting(Settings.FILE_LOGGER, Settings.SECTION_LOGGER_LOGS, entry.getKey(),
              false).setBoolean(settings, value);
    }

    mView.getAdapter().notifyAllSettingsChanged();
  }

  private void convertOnThread(BooleanSupplier f)
  {
    ThreadUtil.runOnThreadAndShowResult(mView.getActivity(), R.string.wii_converting, 0, () ->
            mContext.getResources().getString(
                    f.get() ? R.string.wii_convert_success : R.string.wii_convert_failure));
  }
}
