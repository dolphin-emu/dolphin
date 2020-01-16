package org.dolphinemu.dolphinemu.features.settings.ui;

import android.os.Bundle;
import android.text.TextUtils;

import org.dolphinemu.dolphinemu.DolphinApplication;
import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Setting;
import org.dolphinemu.dolphinemu.features.settings.model.SettingSection;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.CheckBoxSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.HeaderSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.InputBindingSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.RumbleBindingSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.model.view.SingleChoiceSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.SingleChoiceSettingDynamicDescriptions;
import org.dolphinemu.dolphinemu.features.settings.model.view.SliderSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.StringSingleChoiceSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.SubmenuSetting;
import org.dolphinemu.dolphinemu.features.settings.utils.SettingsFile;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.utils.EGLHelper;
import org.dolphinemu.dolphinemu.utils.Log;
import org.dolphinemu.dolphinemu.utils.TvUtil;

import java.io.File;
import java.util.ArrayList;

public final class SettingsFragmentPresenter
{
  private SettingsFragmentView mView;

  public static final String ARG_CONTROLLER_TYPE = "controller_type";
  private MenuTag mMenuTag;
  private String mGameID;

  private Settings mSettings;
  private ArrayList<SettingsItem> mSettingsList;

  private int mControllerNumber;
  private int mControllerType;

  public SettingsFragmentPresenter(SettingsFragmentView view)
  {
    mView = view;
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
    else
    {
      mMenuTag = menuTag;
    }
  }

  public void onViewCreated(MenuTag menuTag, Settings settings)
  {
    this.mMenuTag = menuTag;
    setSettings(settings);
  }

  /**
   * If the screen is rotated, the Activity will forget the settings map. This fragment
   * won't, though; so rather than have the Activity reload from disk, have the fragment pass
   * the settings map back to the Activity.
   */
  public void onAttach()
  {
    if (mSettings != null)
    {
      mView.passSettingsToActivity(mSettings);
    }
  }

  public void putSetting(Setting setting)
  {
    mSettings.getSection(setting.getSection()).putSetting(setting);
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
    if (!TextUtils.isEmpty(mGameID))
    {
      mView.getActivity().setTitle("Game Settings: " + mGameID);
    }
    ArrayList<SettingsItem> sl = new ArrayList<>();

    switch (mMenuTag)
    {
      case CONFIG:
        addConfigSettings(sl);
        break;

      case CONFIG_GENERAL:
        addGeneralSettings(sl);
        break;

      case CONFIG_INTERFACE:
        addInterfaceSettings(sl);
        break;

      case CONFIG_GAME_CUBE:
        addGameCubeSettings(sl);
        break;

      case CONFIG_WII:
        addWiiSettings(sl);
        break;

      case GRAPHICS:
        addGraphicsSettings(sl);
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

      case HACKS:
        addHackSettings(sl);
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

      case STEREOSCOPY:
        addStereoSettings(sl);
        break;

      default:
        mView.showToastMessage("Unimplemented menu");
        return;
    }

    mSettingsList = sl;
    mView.showSettingsList(mSettingsList);
  }

  private void addConfigSettings(ArrayList<SettingsItem> sl)
  {
    sl.add(new SubmenuSetting(null, null, R.string.general_submenu, 0, MenuTag.CONFIG_GENERAL));
    sl.add(new SubmenuSetting(null, null, R.string.interface_submenu, 0, MenuTag.CONFIG_INTERFACE));

    sl.add(new SubmenuSetting(null, null, R.string.gamecube_submenu, 0, MenuTag.CONFIG_GAME_CUBE));
    sl.add(new SubmenuSetting(null, null, R.string.wii_submenu, 0, MenuTag.CONFIG_WII));
    sl.add(new SubmenuSetting(null, null, R.string.debug_submenu, 0, MenuTag.DEBUG));
    sl.add(new HeaderSetting(null, null, R.string.gametdb_thanks, 0));
  }

  private void addGeneralSettings(ArrayList<SettingsItem> sl)
  {
    Setting cpuCore = null;
    Setting dualCore = null;
    Setting overclockEnable = null;
    Setting overclock = null;
    Setting speedLimit = null;
    Setting audioStretch = null;
    Setting audioVolume = null;
    Setting overrideRegionSettings = null;
    Setting autoDiscChange = null;
    Setting analytics = null;
    Setting enableSaveState;

    SettingSection coreSection = mSettings.getSection(Settings.SECTION_INI_CORE);
    SettingSection dspSection = mSettings.getSection(Settings.SECTION_INI_DSP);
    SettingSection analyticsSection = mSettings.getSection(Settings.SECTION_ANALYTICS);
    cpuCore = coreSection.getSetting(SettingsFile.KEY_CPU_CORE);
    dualCore = coreSection.getSetting(SettingsFile.KEY_DUAL_CORE);
    overclockEnable = coreSection.getSetting(SettingsFile.KEY_OVERCLOCK_ENABLE);
    overclock = coreSection.getSetting(SettingsFile.KEY_OVERCLOCK_PERCENT);
    speedLimit = coreSection.getSetting(SettingsFile.KEY_SPEED_LIMIT);
    audioStretch = coreSection.getSetting(SettingsFile.KEY_AUDIO_STRETCH);
    audioVolume = dspSection.getSetting(SettingsFile.KEY_AUDIO_VOLUME);
    overrideRegionSettings = coreSection.getSetting(SettingsFile.KEY_OVERRIDE_REGION_SETTINGS);
    autoDiscChange = coreSection.getSetting(SettingsFile.KEY_AUTO_DISC_CHANGE);
    analytics = analyticsSection.getSetting(SettingsFile.KEY_ANALYTICS_ENABLED);
    enableSaveState = coreSection.getSetting(SettingsFile.KEY_ENABLE_SAVE_STATES);

    // TODO: Having different emuCoresEntries/emuCoresValues for each architecture is annoying.
    // The proper solution would be to have one emuCoresEntries and one emuCoresValues
    // and exclude the values that aren't present in PowerPC::AvailableCPUCores().
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
    sl.add(new SingleChoiceSetting(SettingsFile.KEY_CPU_CORE, Settings.SECTION_INI_CORE,
            R.string.cpu_core, 0, emuCoresEntries, emuCoresValues, defaultCpuCore, cpuCore));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_DUAL_CORE, Settings.SECTION_INI_CORE,
            R.string.dual_core, R.string.dual_core_description, true, dualCore));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_OVERCLOCK_ENABLE, Settings.SECTION_INI_CORE,
            R.string.overclock_enable, R.string.overclock_enable_description, false,
            overclockEnable));
    sl.add(new SliderSetting(SettingsFile.KEY_OVERCLOCK_PERCENT, Settings.SECTION_INI_CORE,
            R.string.overclock_title, R.string.overclock_title_description, 400, "%", 100,
            overclock));
    sl.add(new SliderSetting(SettingsFile.KEY_SPEED_LIMIT, Settings.SECTION_INI_CORE,
            R.string.speed_limit, 0, 200, "%", 100, speedLimit));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_AUDIO_STRETCH, Settings.SECTION_INI_CORE,
            R.string.audio_stretch, R.string.audio_stretch_description, false, audioStretch));
    sl.add(new SliderSetting(SettingsFile.KEY_AUDIO_VOLUME, Settings.SECTION_INI_DSP,
            R.string.audio_volume, 0, 100, "%", 100, audioVolume));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_OVERRIDE_REGION_SETTINGS,
            Settings.SECTION_INI_CORE, R.string.override_region_settings, 0, false,
            overrideRegionSettings));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_AUTO_DISC_CHANGE, Settings.SECTION_INI_CORE,
            R.string.auto_disc_change, 0, false, autoDiscChange));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_ENABLE_SAVE_STATES, Settings.SECTION_INI_CORE,
            R.string.enable_save_states, R.string.enable_save_states_description, false,
            enableSaveState));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_ANALYTICS_ENABLED, Settings.SECTION_ANALYTICS,
            R.string.analytics, 0, false, analytics));
  }

  private void addInterfaceSettings(ArrayList<SettingsItem> sl)
  {
    Setting usePanicHandlers = null;
    Setting onScreenDisplayMessages = null;

    usePanicHandlers = mSettings.getSection(Settings.SECTION_INI_INTERFACE)
            .getSetting(SettingsFile.KEY_USE_PANIC_HANDLERS);
    onScreenDisplayMessages = mSettings.getSection(Settings.SECTION_INI_INTERFACE)
            .getSetting(SettingsFile.KEY_OSD_MESSAGES);

    sl.add(new CheckBoxSetting(SettingsFile.KEY_USE_PANIC_HANDLERS, Settings.SECTION_INI_INTERFACE,
            R.string.panic_handlers, R.string.panic_handlers_description, true, usePanicHandlers));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_OSD_MESSAGES, Settings.SECTION_INI_INTERFACE,
            R.string.osd_messages, R.string.osd_messages_description, true,
            onScreenDisplayMessages));
  }

  private void addGameCubeSettings(ArrayList<SettingsItem> sl)
  {
    Setting systemLanguage = null;
    Setting slotADevice = null;
    Setting slotBDevice = null;

    SettingSection coreSection = mSettings.getSection(Settings.SECTION_INI_CORE);
    systemLanguage = coreSection.getSetting(SettingsFile.KEY_GAME_CUBE_LANGUAGE);
    slotADevice = coreSection.getSetting(SettingsFile.KEY_SLOT_A_DEVICE);
    slotBDevice = coreSection.getSetting(SettingsFile.KEY_SLOT_B_DEVICE);

    sl.add(new SingleChoiceSetting(SettingsFile.KEY_GAME_CUBE_LANGUAGE, Settings.SECTION_INI_CORE,
            R.string.gamecube_system_language, 0, R.array.gameCubeSystemLanguageEntries,
            R.array.gameCubeSystemLanguageValues, 0, systemLanguage));
    sl.add(new SingleChoiceSetting(SettingsFile.KEY_SLOT_A_DEVICE, Settings.SECTION_INI_CORE,
            R.string.slot_a_device, 0, R.array.slotDeviceEntries, R.array.slotDeviceValues, 8,
            slotADevice));
    sl.add(new SingleChoiceSetting(SettingsFile.KEY_SLOT_B_DEVICE, Settings.SECTION_INI_CORE,
            R.string.slot_b_device, 0, R.array.slotDeviceEntries, R.array.slotDeviceValues, 255,
            slotBDevice));
  }

  private void addWiiSettings(ArrayList<SettingsItem> sl)
  {
    Setting continuousScan = null;
    Setting wiimoteSpeaker = null;

    SettingSection coreSection = mSettings.getSection(Settings.SECTION_INI_CORE);
    continuousScan = coreSection.getSetting(SettingsFile.KEY_WIIMOTE_SCAN);
    wiimoteSpeaker = coreSection.getSetting(SettingsFile.KEY_WIIMOTE_SPEAKER);

    sl.add(new CheckBoxSetting(SettingsFile.KEY_WIIMOTE_SCAN, Settings.SECTION_INI_CORE,
            R.string.wiimote_scanning, R.string.wiimote_scanning_description, true,
            continuousScan));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_WIIMOTE_SPEAKER, Settings.SECTION_INI_CORE,
            R.string.wiimote_speaker, R.string.wiimote_speaker_description, true, wiimoteSpeaker));
  }

  private void addGcPadSettings(ArrayList<SettingsItem> sl)
  {
    for (int i = 0; i < 4; i++)
    {
      if (mGameID.equals(""))
      {
        // TODO This controller_0 + i business is quite the hack. It should work, but only if the definitions are kept together and in order.
        Setting gcPadSetting = mSettings.getSection(Settings.SECTION_INI_CORE)
                .getSetting(SettingsFile.KEY_GCPAD_TYPE + i);
        sl.add(new SingleChoiceSetting(SettingsFile.KEY_GCPAD_TYPE + i, Settings.SECTION_INI_CORE,
                R.string.controller_0 + i, 0, R.array.gcpadTypeEntries, R.array.gcpadTypeValues, 0,
                gcPadSetting, MenuTag.getGCPadMenuTag(i)));
      }
      else
      {
        Setting gcPadSetting = mSettings.getSection(Settings.SECTION_CONTROLS)
                .getSetting(SettingsFile.KEY_GCPAD_G_TYPE + i);
        sl.add(new SingleChoiceSetting(SettingsFile.KEY_GCPAD_G_TYPE + i, Settings.SECTION_CONTROLS,
                R.string.controller_0 + i, 0, R.array.gcpadTypeEntries, R.array.gcpadTypeValues, 0,
                gcPadSetting, MenuTag.getGCPadMenuTag(i)));
      }
    }
  }

  private void addWiimoteSettings(ArrayList<SettingsItem> sl)
  {
    for (int i = 0; i < 4; i++)
    {
      // TODO This wiimote_0 + i business is quite the hack. It should work, but only if the definitions are kept together and in order.
      if (mGameID.equals(""))
      {
        Setting wiimoteSetting = mSettings.getSection(Settings.SECTION_WIIMOTE + (i + 1))
                .getSetting(SettingsFile.KEY_WIIMOTE_TYPE);
        sl.add(new SingleChoiceSetting(SettingsFile.KEY_WIIMOTE_TYPE,
                Settings.SECTION_WIIMOTE + (i + 1), R.string.wiimote_4 + i, 0,
                R.array.wiimoteTypeEntries, R.array.wiimoteTypeValues, 0, wiimoteSetting,
                MenuTag.getWiimoteMenuTag(i + 4)));
      }
      else
      {
        Setting wiimoteSetting = mSettings.getSection(Settings.SECTION_CONTROLS)
                .getSetting(SettingsFile.KEY_WIIMOTE_G_TYPE + i);
        sl.add(new SingleChoiceSetting(SettingsFile.KEY_WIIMOTE_G_TYPE + i,
                Settings.SECTION_CONTROLS, R.string.wiimote_4 + i, 0, R.array.wiimoteTypeEntries,
                R.array.wiimoteTypeValues, 0, wiimoteSetting, MenuTag.getWiimoteMenuTag(i + 4)));
      }
    }
  }

  private void addGraphicsSettings(ArrayList<SettingsItem> sl)
  {
    IntSetting videoBackend =
            new IntSetting(SettingsFile.KEY_VIDEO_BACKEND_INDEX, Settings.SECTION_INI_CORE,
                    getVideoBackendValue());
    Setting showFps = null;
    Setting shaderCompilationMode = null;
    Setting waitForShaders = null;
    Setting aspectRatio = null;

    SettingSection gfxSection = mSettings.getSection(Settings.SECTION_GFX_SETTINGS);
    showFps = gfxSection.getSetting(SettingsFile.KEY_SHOW_FPS);
    shaderCompilationMode = gfxSection.getSetting(SettingsFile.KEY_SHADER_COMPILATION_MODE);
    waitForShaders = gfxSection.getSetting(SettingsFile.KEY_WAIT_FOR_SHADERS);
    aspectRatio = gfxSection.getSetting(SettingsFile.KEY_ASPECT_RATIO);

    sl.add(new HeaderSetting(null, null, R.string.graphics_general, 0));
    sl.add(new SingleChoiceSetting(SettingsFile.KEY_VIDEO_BACKEND_INDEX, Settings.SECTION_INI_CORE,
            R.string.video_backend, 0, R.array.videoBackendEntries,
            R.array.videoBackendValues, 0, videoBackend));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_SHOW_FPS, Settings.SECTION_GFX_SETTINGS,
            R.string.show_fps, R.string.show_fps_description, false, showFps));
    sl.add(new SingleChoiceSettingDynamicDescriptions(SettingsFile.KEY_SHADER_COMPILATION_MODE,
            Settings.SECTION_GFX_SETTINGS, R.string.shader_compilation_mode, 0,
            R.array.shaderCompilationModeEntries,
            R.array.shaderCompilationModeValues, R.array.shaderCompilationDescriptionEntries,
            R.array.shaderCompilationDescriptionValues, 0, shaderCompilationMode));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_WAIT_FOR_SHADERS, Settings.SECTION_GFX_SETTINGS,
            R.string.wait_for_shaders, R.string.wait_for_shaders_description, false,
            waitForShaders));
    sl.add(new SingleChoiceSetting(SettingsFile.KEY_ASPECT_RATIO, Settings.SECTION_GFX_SETTINGS,
            R.string.aspect_ratio, 0, R.array.aspectRatioEntries,
            R.array.aspectRatioValues, 0, aspectRatio));

    sl.add(new HeaderSetting(null, null, R.string.graphics_enhancements_and_hacks, 0));
    sl.add(new SubmenuSetting(null, null, R.string.enhancements_submenu, 0, MenuTag.ENHANCEMENTS));
    sl.add(new SubmenuSetting(null, null, R.string.hacks_submenu, 0, MenuTag.HACKS));
  }

  private void addEnhanceSettings(ArrayList<SettingsItem> sl)
  {
    SettingSection gfxSection = mSettings.getSection(Settings.SECTION_GFX_SETTINGS);
    SettingSection enhancementSection = mSettings.getSection(Settings.SECTION_GFX_ENHANCEMENTS);
    SettingSection hacksSection = mSettings.getSection(Settings.SECTION_GFX_HACKS);

    Setting resolution = gfxSection.getSetting(SettingsFile.KEY_INTERNAL_RES);
    Setting fsaa = gfxSection.getSetting(SettingsFile.KEY_FSAA);
    Setting anisotropic = enhancementSection.getSetting(SettingsFile.KEY_ANISOTROPY);
    Setting shader = enhancementSection.getSetting(SettingsFile.KEY_POST_SHADER);
    Setting efbScaledCopy = hacksSection.getSetting(SettingsFile.KEY_SCALED_EFB);
    Setting perPixel = gfxSection.getSetting(SettingsFile.KEY_PER_PIXEL);
    Setting forceFilter = enhancementSection.getSetting(SettingsFile.KEY_FORCE_FILTERING);
    Setting disableFog = gfxSection.getSetting(SettingsFile.KEY_DISABLE_FOG);
    Setting disableCopyFilter = enhancementSection.getSetting(SettingsFile.KEY_DISABLE_COPY_FILTER);
    Setting arbitraryMipmapDetection =
            enhancementSection.getSetting(SettingsFile.KEY_ARBITRARY_MIPMAP_DETECTION);
    Setting wideScreenHack = gfxSection.getSetting(SettingsFile.KEY_WIDE_SCREEN_HACK);
    Setting force24BitColor = enhancementSection.getSetting(SettingsFile.KEY_FORCE_24_BIT_COLOR);
    Setting backendMultithreading = gfxSection.getSetting(SettingsFile.KEY_BACKEND_MULTITHREADING);

    sl.add(new SingleChoiceSetting(SettingsFile.KEY_INTERNAL_RES, Settings.SECTION_GFX_SETTINGS,
            R.string.internal_resolution, R.string.internal_resolution_description,
            R.array.internalResolutionEntries, R.array.internalResolutionValues, 1, resolution));
    sl.add(new SingleChoiceSetting(SettingsFile.KEY_FSAA, Settings.SECTION_GFX_SETTINGS,
            R.string.FSAA, R.string.FSAA_description, R.array.FSAAEntries, R.array.FSAAValues, 1,
            fsaa));
    sl.add(new SingleChoiceSetting(SettingsFile.KEY_ANISOTROPY, Settings.SECTION_GFX_ENHANCEMENTS,
            R.string.anisotropic_filtering, R.string.anisotropic_filtering_description,
            R.array.anisotropicFilteringEntries, R.array.anisotropicFilteringValues, 0,
            anisotropic));

    IntSetting stereoModeValue = (IntSetting) gfxSection.getSetting(SettingsFile.KEY_STEREO_MODE);
    int anaglyphMode = 3;
    String subDir =
            stereoModeValue != null && stereoModeValue.getValue() == anaglyphMode ? "Anaglyph" :
                    null;
    String[] shaderListEntries = getShaderList(subDir);
    String[] shaderListValues = new String[shaderListEntries.length];
    System.arraycopy(shaderListEntries, 0, shaderListValues, 0, shaderListEntries.length);
    shaderListValues[0] = "";
    sl.add(new StringSingleChoiceSetting(SettingsFile.KEY_POST_SHADER,
            Settings.SECTION_GFX_ENHANCEMENTS, R.string.post_processing_shader,
            0, shaderListEntries, shaderListValues, "",
            shader));

    sl.add(new CheckBoxSetting(SettingsFile.KEY_SCALED_EFB, Settings.SECTION_GFX_HACKS,
            R.string.scaled_efb_copy, R.string.scaled_efb_copy_description, true, efbScaledCopy));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_PER_PIXEL, Settings.SECTION_GFX_SETTINGS,
            R.string.per_pixel_lighting, R.string.per_pixel_lighting_description, false, perPixel));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_FORCE_FILTERING, Settings.SECTION_GFX_ENHANCEMENTS,
            R.string.force_texture_filtering, R.string.force_texture_filtering_description, false,
            forceFilter));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_FORCE_24_BIT_COLOR,
            Settings.SECTION_GFX_ENHANCEMENTS,
            R.string.force_24bit_color, R.string.force_24bit_color_description, true,
            force24BitColor));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_DISABLE_FOG, Settings.SECTION_GFX_SETTINGS,
            R.string.disable_fog, R.string.disable_fog_description, false, disableFog));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_DISABLE_COPY_FILTER,
            Settings.SECTION_GFX_ENHANCEMENTS,
            R.string.disable_copy_filter, R.string.disable_copy_filter_description, false,
            disableCopyFilter));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_ARBITRARY_MIPMAP_DETECTION,
            Settings.SECTION_GFX_ENHANCEMENTS, R.string.arbitrary_mipmap_detection,
            R.string.arbitrary_mipmap_detection_description, true, arbitraryMipmapDetection));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_WIDE_SCREEN_HACK, Settings.SECTION_GFX_SETTINGS,
            R.string.wide_screen_hack, R.string.wide_screen_hack_description, false,
            wideScreenHack));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_BACKEND_MULTITHREADING,
            Settings.SECTION_GFX_SETTINGS,
            R.string.backend_multithreading, R.string.backend_multithreading_description, false,
            backendMultithreading));

     /*
     Check if we support stereo
     If we support desktop GL then we must support at least OpenGL 3.2
     If we only support OpenGLES then we need both OpenGLES 3.1 and AEP
     */
    EGLHelper helper = new EGLHelper(EGLHelper.EGL_OPENGL_ES2_BIT);

    if ((helper.supportsOpenGL() && helper.GetVersion() >= 320) ||
            (helper.supportsGLES3() && helper.GetVersion() >= 310 &&
                    helper.SupportsExtension("GL_ANDROID_extension_pack_es31a")))
    {
      sl.add(new SubmenuSetting(SettingsFile.KEY_STEREO_MODE, null, R.string.stereoscopy_submenu,
              R.string.stereoscopy_submenu_description, MenuTag.STEREOSCOPY));
    }
  }

  private String[] getShaderList(String subDir)
  {
    try
    {
      String shadersPath =
              DirectoryInitialization.getDolphinInternalDirectory() + "/Shaders";
      if (!TextUtils.isEmpty(subDir))
      {
        shadersPath += "/" + subDir;
      }

      File file = new File(shadersPath);
      File[] shaderFiles = file.listFiles();
      if (shaderFiles != null)
      {
        String[] result = new String[shaderFiles.length + 1];
        result[0] = mView.getActivity().getString(R.string.off);
        for (int i = 0; i < shaderFiles.length; i++)
        {
          String name = shaderFiles[i].getName();
          int extensionIndex = name.indexOf(".glsl");
          if (extensionIndex > 0)
          {
            name = name.substring(0, extensionIndex);
          }
          result[i + 1] = name;
        }

        return result;
      }
    }
    catch (Exception ex)
    {
      Log.debug("[Settings] Unable to find shader files");
      // return empty list
    }

    return new String[]{};
  }

  private void addHackSettings(ArrayList<SettingsItem> sl)
  {
    SettingSection gfxSection = mSettings.getSection(Settings.SECTION_GFX_SETTINGS);
    SettingSection hacksSection = mSettings.getSection(Settings.SECTION_GFX_HACKS);

    boolean skipEFBValue =
            getInvertedBooleanValue(Settings.SECTION_GFX_HACKS, SettingsFile.KEY_SKIP_EFB, false);
    boolean ignoreFormatValue =
            getInvertedBooleanValue(Settings.SECTION_GFX_HACKS, SettingsFile.KEY_IGNORE_FORMAT,
                    true);

    BooleanSetting skipEFB =
            new BooleanSetting(SettingsFile.KEY_SKIP_EFB, Settings.SECTION_GFX_HACKS, skipEFBValue);
    BooleanSetting ignoreFormat =
            new BooleanSetting(SettingsFile.KEY_IGNORE_FORMAT, Settings.SECTION_GFX_HACKS,
                    ignoreFormatValue);
    Setting efbToTexture = hacksSection.getSetting(SettingsFile.KEY_EFB_TEXTURE);
    Setting deferEfbCopies = hacksSection.getSetting(SettingsFile.KEY_DEFER_EFB_COPIES);
    Setting texCacheAccuracy = gfxSection.getSetting(SettingsFile.KEY_TEXCACHE_ACCURACY);
    Setting gpuTextureDecoding = gfxSection.getSetting(SettingsFile.KEY_GPU_TEXTURE_DECODING);
    Setting xfbToTexture = hacksSection.getSetting(SettingsFile.KEY_XFB_TEXTURE);
    Setting immediateXfb = hacksSection.getSetting(SettingsFile.KEY_IMMEDIATE_XFB);
    Setting skipDuplicateXfbs = hacksSection.getSetting(SettingsFile.KEY_SKIP_DUPLICATE_XFBS);
    Setting fastDepth = gfxSection.getSetting(SettingsFile.KEY_FAST_DEPTH);

    sl.add(new HeaderSetting(null, null, R.string.embedded_frame_buffer, 0));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_SKIP_EFB, Settings.SECTION_GFX_HACKS,
            R.string.skip_efb_access, R.string.skip_efb_access_description, false, skipEFB));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_IGNORE_FORMAT, Settings.SECTION_GFX_HACKS,
            R.string.ignore_format_changes, R.string.ignore_format_changes_description, true,
            ignoreFormat));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_EFB_TEXTURE, Settings.SECTION_GFX_HACKS,
            R.string.efb_copy_method, R.string.efb_copy_method_description, true, efbToTexture));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_DEFER_EFB_COPIES, Settings.SECTION_GFX_HACKS,
            R.string.defer_efb_copies, R.string.defer_efb_copies_description, true,
            deferEfbCopies));

    sl.add(new HeaderSetting(null, null, R.string.texture_cache, 0));
    sl.add(new SingleChoiceSetting(SettingsFile.KEY_TEXCACHE_ACCURACY,
            Settings.SECTION_GFX_SETTINGS, R.string.texture_cache_accuracy,
            R.string.texture_cache_accuracy_description, R.array.textureCacheAccuracyEntries,
            R.array.textureCacheAccuracyValues, 128, texCacheAccuracy));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_GPU_TEXTURE_DECODING, Settings.SECTION_GFX_SETTINGS,
            R.string.gpu_texture_decoding, R.string.gpu_texture_decoding_description, false,
            gpuTextureDecoding));

    sl.add(new HeaderSetting(null, null, R.string.external_frame_buffer, 0));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_XFB_TEXTURE, Settings.SECTION_GFX_HACKS,
            R.string.xfb_copy_method, R.string.xfb_copy_method_description, true, xfbToTexture));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_IMMEDIATE_XFB, Settings.SECTION_GFX_HACKS,
            R.string.immediate_xfb, R.string.immediate_xfb_description, false, immediateXfb));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_SKIP_DUPLICATE_XFBS, Settings.SECTION_GFX_HACKS,
            R.string.skip_duplicate_xfbs, R.string.skip_duplicate_xfbs_description, true,
            skipDuplicateXfbs));

    sl.add(new HeaderSetting(null, null, R.string.other, 0));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_FAST_DEPTH, Settings.SECTION_GFX_SETTINGS,
            R.string.fast_depth_calculation, R.string.fast_depth_calculation_description, true,
            fastDepth));
  }

  private void addDebugSettings(ArrayList<SettingsItem> sl)
  {
    SettingSection debugSection = mSettings.getSection(Settings.SECTION_DEBUG);

    Setting jitOff = debugSection.getSetting(SettingsFile.KEY_DEBUG_JITOFF);
    Setting jitLoadStoreOff = debugSection.getSetting(SettingsFile.KEY_DEBUG_JITLOADSTOREOFF);
    Setting jitLoadStoreFloatingPointOff =
            debugSection.getSetting(SettingsFile.KEY_DEBUG_JITLOADSTOREFLOATINGPOINTOFF);
    Setting jitLoadStorePairedOff =
            debugSection.getSetting(SettingsFile.KEY_DEBUG_JITLOADSTOREPAIREDOFF);
    Setting jitFloatingPointOff =
            debugSection.getSetting(SettingsFile.KEY_DEBUG_JITFLOATINGPOINTOFF);
    Setting jitIntegerOff = debugSection.getSetting(SettingsFile.KEY_DEBUG_JITINTEGEROFF);
    Setting jitPairedOff = debugSection.getSetting(SettingsFile.KEY_DEBUG_JITPAIREDOFF);
    Setting jitSystemRegistersOff =
            debugSection.getSetting(SettingsFile.KEY_DEBUG_JITSYSTEMREGISTEROFF);
    Setting jitBranchOff = debugSection.getSetting(SettingsFile.KEY_DEBUG_JITBRANCHOFF);

    sl.add(new HeaderSetting(null, null, R.string.debug_warning, 0));

    sl.add(new HeaderSetting(null, null, R.string.debug_jit_header, 0));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_DEBUG_JITOFF, Settings.SECTION_DEBUG,
            R.string.debug_jitoff, 0, false,
            jitOff));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_DEBUG_JITLOADSTOREOFF, Settings.SECTION_DEBUG,
            R.string.debug_jitloadstoreoff, 0, false,
            jitLoadStoreOff));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_DEBUG_JITLOADSTOREFLOATINGPOINTOFF,
            Settings.SECTION_DEBUG,
            R.string.debug_jitloadstorefloatingoff, 0, false,
            jitLoadStoreFloatingPointOff));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_DEBUG_JITLOADSTOREPAIREDOFF, Settings.SECTION_DEBUG,
            R.string.debug_jitloadstorepairedoff, 0, false,
            jitLoadStorePairedOff));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_DEBUG_JITFLOATINGPOINTOFF, Settings.SECTION_DEBUG,
            R.string.debug_jitfloatingpointoff, 0, false,
            jitFloatingPointOff));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_DEBUG_JITINTEGEROFF, Settings.SECTION_DEBUG,
            R.string.debug_jitintegeroff, 0, false,
            jitIntegerOff));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_DEBUG_JITPAIREDOFF, Settings.SECTION_DEBUG,
            R.string.debug_jitpairedoff, 0, false,
            jitPairedOff));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_DEBUG_JITSYSTEMREGISTEROFF, Settings.SECTION_DEBUG,
            R.string.debug_jitsystemregistersoffr, 0, false,
            jitSystemRegistersOff));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_DEBUG_JITBRANCHOFF, Settings.SECTION_DEBUG,
            R.string.debug_jitbranchoff, 0, false,
            jitBranchOff));
  }

  private void addStereoSettings(ArrayList<SettingsItem> sl)
  {
    SettingSection stereoScopySection = mSettings.getSection(Settings.SECTION_STEREOSCOPY);

    Setting stereoModeValue = stereoScopySection.getSetting(SettingsFile.KEY_STEREO_MODE);
    Setting stereoDepth = stereoScopySection.getSetting(SettingsFile.KEY_STEREO_DEPTH);
    Setting convergence = stereoScopySection.getSetting(SettingsFile.KEY_STEREO_CONV);
    Setting swapEyes = stereoScopySection.getSetting(SettingsFile.KEY_STEREO_SWAP);

    sl.add(new SingleChoiceSetting(SettingsFile.KEY_STEREO_MODE, Settings.SECTION_STEREOSCOPY,
            R.string.stereoscopy_mode, 0,
            R.array.stereoscopyEntries, R.array.stereoscopyValues, 0, stereoModeValue));
    sl.add(new SliderSetting(SettingsFile.KEY_STEREO_DEPTH, Settings.SECTION_STEREOSCOPY,
            R.string.stereoscopy_depth, R.string.stereoscopy_depth_description, 100, "%", 20,
            stereoDepth));
    sl.add(new SliderSetting(SettingsFile.KEY_STEREO_CONV, Settings.SECTION_STEREOSCOPY,
            R.string.stereoscopy_convergence, R.string.stereoscopy_convergence_description, 200,
            "%", 0, convergence));
    sl.add(new CheckBoxSetting(SettingsFile.KEY_STEREO_SWAP, Settings.SECTION_STEREOSCOPY,
            R.string.stereoscopy_swap_eyes, R.string.stereoscopy_swap_eyes_description, false,
            swapEyes));
  }

  private void addGcPadSubSettings(ArrayList<SettingsItem> sl, int gcPadNumber, int gcPadType)
  {
    SettingSection bindingsSection = mSettings.getSection(Settings.SECTION_BINDINGS);
    SettingSection coreSection = mSettings.getSection(Settings.SECTION_INI_CORE);

    if (gcPadType == 1) // Emulated
    {
      Setting bindA = bindingsSection.getSetting(SettingsFile.KEY_GCBIND_A + gcPadNumber);
      Setting bindB = bindingsSection.getSetting(SettingsFile.KEY_GCBIND_B + gcPadNumber);
      Setting bindX = bindingsSection.getSetting(SettingsFile.KEY_GCBIND_X + gcPadNumber);
      Setting bindY = bindingsSection.getSetting(SettingsFile.KEY_GCBIND_Y + gcPadNumber);
      Setting bindZ = bindingsSection.getSetting(SettingsFile.KEY_GCBIND_Z + gcPadNumber);
      Setting bindStart = bindingsSection.getSetting(SettingsFile.KEY_GCBIND_START + gcPadNumber);
      Setting bindControlUp =
              bindingsSection.getSetting(SettingsFile.KEY_GCBIND_CONTROL_UP + gcPadNumber);
      Setting bindControlDown =
              bindingsSection.getSetting(SettingsFile.KEY_GCBIND_CONTROL_DOWN + gcPadNumber);
      Setting bindControlLeft =
              bindingsSection.getSetting(SettingsFile.KEY_GCBIND_CONTROL_LEFT + gcPadNumber);
      Setting bindControlRight =
              bindingsSection.getSetting(SettingsFile.KEY_GCBIND_CONTROL_RIGHT + gcPadNumber);
      Setting bindCUp = bindingsSection.getSetting(SettingsFile.KEY_GCBIND_C_UP + gcPadNumber);
      Setting bindCDown = bindingsSection.getSetting(SettingsFile.KEY_GCBIND_C_DOWN + gcPadNumber);
      Setting bindCLeft = bindingsSection.getSetting(SettingsFile.KEY_GCBIND_C_LEFT + gcPadNumber);
      Setting bindCRight =
              bindingsSection.getSetting(SettingsFile.KEY_GCBIND_C_RIGHT + gcPadNumber);
      Setting bindTriggerL =
              bindingsSection.getSetting(SettingsFile.KEY_GCBIND_TRIGGER_L + gcPadNumber);
      Setting bindTriggerR =
              bindingsSection.getSetting(SettingsFile.KEY_GCBIND_TRIGGER_R + gcPadNumber);
      Setting bindDPadUp =
              bindingsSection.getSetting(SettingsFile.KEY_GCBIND_DPAD_UP + gcPadNumber);
      Setting bindDPadDown =
              bindingsSection.getSetting(SettingsFile.KEY_GCBIND_DPAD_DOWN + gcPadNumber);
      Setting bindDPadLeft =
              bindingsSection.getSetting(SettingsFile.KEY_GCBIND_DPAD_LEFT + gcPadNumber);
      Setting bindDPadRight =
              bindingsSection.getSetting(SettingsFile.KEY_GCBIND_DPAD_RIGHT + gcPadNumber);
      Setting gcEmuRumble =
              bindingsSection.getSetting(SettingsFile.KEY_EMU_RUMBLE + gcPadNumber);

      sl.add(new HeaderSetting(null, null, R.string.generic_buttons, 0));
      sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_A + gcPadNumber,
              Settings.SECTION_BINDINGS, R.string.button_a, bindA, mGameID));
      sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_B + gcPadNumber,
              Settings.SECTION_BINDINGS, R.string.button_b, bindB, mGameID));
      sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_X + gcPadNumber,
              Settings.SECTION_BINDINGS, R.string.button_x, bindX, mGameID));
      sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_Y + gcPadNumber,
              Settings.SECTION_BINDINGS, R.string.button_y, bindY, mGameID));
      sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_Z + gcPadNumber,
              Settings.SECTION_BINDINGS, R.string.button_z, bindZ, mGameID));
      sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_START + gcPadNumber,
              Settings.SECTION_BINDINGS, R.string.button_start, bindStart, mGameID));

      sl.add(new HeaderSetting(null, null, R.string.controller_control, 0));
      sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_CONTROL_UP + gcPadNumber,
              Settings.SECTION_BINDINGS, R.string.generic_up, bindControlUp, mGameID));
      sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_CONTROL_DOWN + gcPadNumber,
              Settings.SECTION_BINDINGS, R.string.generic_down, bindControlDown, mGameID));
      sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_CONTROL_LEFT + gcPadNumber,
              Settings.SECTION_BINDINGS, R.string.generic_left, bindControlLeft, mGameID));
      sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_CONTROL_RIGHT + gcPadNumber,
              Settings.SECTION_BINDINGS, R.string.generic_right, bindControlRight, mGameID));

      sl.add(new HeaderSetting(null, null, R.string.controller_c, 0));
      sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_C_UP + gcPadNumber,
              Settings.SECTION_BINDINGS, R.string.generic_up, bindCUp, mGameID));
      sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_C_DOWN + gcPadNumber,
              Settings.SECTION_BINDINGS, R.string.generic_down, bindCDown, mGameID));
      sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_C_LEFT + gcPadNumber,
              Settings.SECTION_BINDINGS, R.string.generic_left, bindCLeft, mGameID));
      sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_C_RIGHT + gcPadNumber,
              Settings.SECTION_BINDINGS, R.string.generic_right, bindCRight, mGameID));

      sl.add(new HeaderSetting(null, null, R.string.controller_trig, 0));
      sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_TRIGGER_L + gcPadNumber,
              Settings.SECTION_BINDINGS, R.string.trigger_left, bindTriggerL, mGameID));
      sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_TRIGGER_R + gcPadNumber,
              Settings.SECTION_BINDINGS, R.string.trigger_right, bindTriggerR, mGameID));

      sl.add(new HeaderSetting(null, null, R.string.controller_dpad, 0));
      sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_DPAD_UP + gcPadNumber,
              Settings.SECTION_BINDINGS, R.string.generic_up, bindDPadUp, mGameID));
      sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_DPAD_DOWN + gcPadNumber,
              Settings.SECTION_BINDINGS, R.string.generic_down, bindDPadDown, mGameID));
      sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_DPAD_LEFT + gcPadNumber,
              Settings.SECTION_BINDINGS, R.string.generic_left, bindDPadLeft, mGameID));
      sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_DPAD_RIGHT + gcPadNumber,
              Settings.SECTION_BINDINGS, R.string.generic_right, bindDPadRight, mGameID));


      sl.add(new HeaderSetting(null, null, R.string.emulation_control_rumble, 0));
      sl.add(new RumbleBindingSetting(SettingsFile.KEY_EMU_RUMBLE + gcPadNumber,
              Settings.SECTION_BINDINGS, R.string.emulation_control_rumble, gcEmuRumble, mGameID));
    }
    else // Adapter
    {
      Setting rumble = coreSection.getSetting(SettingsFile.KEY_GCADAPTER_RUMBLE + gcPadNumber);
      Setting bongos = coreSection.getSetting(SettingsFile.KEY_GCADAPTER_BONGOS + gcPadNumber);

      sl.add(new CheckBoxSetting(SettingsFile.KEY_GCADAPTER_RUMBLE + gcPadNumber,
              Settings.SECTION_INI_CORE, R.string.gc_adapter_rumble,
              R.string.gc_adapter_rumble_description, false, rumble));
      sl.add(new CheckBoxSetting(SettingsFile.KEY_GCADAPTER_BONGOS + gcPadNumber,
              Settings.SECTION_INI_CORE, R.string.gc_adapter_bongos,
              R.string.gc_adapter_bongos_description, false, bongos));
    }
  }

  private void addWiimoteSubSettings(ArrayList<SettingsItem> sl, int wiimoteNumber)
  {
    SettingSection bindingsSection = mSettings.getSection(Settings.SECTION_BINDINGS);


    Setting bindA = bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_A + wiimoteNumber);
    Setting bindB = bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_B + wiimoteNumber);
    Setting bind1 = bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_1 + wiimoteNumber);
    Setting bind2 = bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_2 + wiimoteNumber);
    Setting bindMinus = bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_MINUS + wiimoteNumber);
    Setting bindPlus = bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_PLUS + wiimoteNumber);
    Setting bindHome = bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_HOME + wiimoteNumber);
    Setting bindIRUp = bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_IR_UP + wiimoteNumber);
    Setting bindIRDown =
            bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_IR_DOWN + wiimoteNumber);
    Setting bindIRLeft =
            bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_IR_LEFT + wiimoteNumber);
    Setting bindIRRight =
            bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_IR_RIGHT + wiimoteNumber);
    Setting bindIRForward =
            bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_IR_FORWARD + wiimoteNumber);
    Setting bindIRBackward =
            bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_IR_BACKWARD + wiimoteNumber);
    Setting bindIRHide =
            bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_IR_HIDE + wiimoteNumber);
    Setting bindSwingUp =
            bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_SWING_UP + wiimoteNumber);
    Setting bindSwingDown =
            bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_SWING_DOWN + wiimoteNumber);
    Setting bindSwingLeft =
            bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_SWING_LEFT + wiimoteNumber);
    Setting bindSwingRight =
            bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_SWING_RIGHT + wiimoteNumber);
    Setting bindSwingForward =
            bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_SWING_FORWARD + wiimoteNumber);
    Setting bindSwingBackward =
            bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_SWING_BACKWARD + wiimoteNumber);
    Setting bindTiltForward =
            bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_TILT_FORWARD + wiimoteNumber);
    Setting bindTiltBackward =
            bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_TILT_BACKWARD + wiimoteNumber);
    Setting bindTiltLeft =
            bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_TILT_LEFT + wiimoteNumber);
    Setting bindTiltRight =
            bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_TILT_RIGHT + wiimoteNumber);
    Setting bindTiltModifier =
            bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_TILT_MODIFIER + wiimoteNumber);
    Setting bindShakeX =
            bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_SHAKE_X + wiimoteNumber);
    Setting bindShakeY =
            bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_SHAKE_Y + wiimoteNumber);
    Setting bindShakeZ =
            bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_SHAKE_Z + wiimoteNumber);
    Setting bindDPadUp =
            bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_DPAD_UP + wiimoteNumber);
    Setting bindDPadDown =
            bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_DPAD_DOWN + wiimoteNumber);
    Setting bindDPadLeft =
            bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_DPAD_LEFT + wiimoteNumber);
    Setting bindDPadRight =
            bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_DPAD_RIGHT + wiimoteNumber);
    Setting wiiEmuRumble =
            bindingsSection.getSetting(SettingsFile.KEY_EMU_RUMBLE + wiimoteNumber);

    // Bindings use controller numbers 4-7 (0-3 are GameCube), but the extension setting uses 1-4.
    // But game game specific extension settings are saved in their own profile. These profiles
    // do not have any way to specify the controller that is loaded outside of knowing the filename
    // of the profile that was loaded.
    IntSetting extension;
    if (mGameID.equals(""))
    {
      extension = new IntSetting(SettingsFile.KEY_WIIMOTE_EXTENSION,
              Settings.SECTION_WIIMOTE + wiimoteNumber, getExtensionValue(wiimoteNumber - 3),
              MenuTag.getWiimoteExtensionMenuTag(wiimoteNumber));
      sl.add(new SingleChoiceSetting(SettingsFile.KEY_WIIMOTE_EXTENSION,
              Settings.SECTION_WIIMOTE + (wiimoteNumber - 3), R.string.wiimote_extensions,
              0, R.array.wiimoteExtensionsEntries,
              R.array.wiimoteExtensionsValues, 0, extension,
              MenuTag.getWiimoteExtensionMenuTag(wiimoteNumber)));
    }
    else
    {
      mSettings.loadWiimoteProfile(mGameID, String.valueOf(wiimoteNumber - 4));
      extension = new IntSetting(SettingsFile.KEY_WIIMOTE_EXTENSION + (wiimoteNumber - 4),
              Settings.SECTION_CONTROLS, getExtensionValue(wiimoteNumber - 4),
              MenuTag.getWiimoteExtensionMenuTag(wiimoteNumber));
      sl.add(new SingleChoiceSetting(SettingsFile.KEY_WIIMOTE_EXTENSION + (wiimoteNumber - 4),
              Settings.SECTION_CONTROLS, R.string.wiimote_extensions,
              0, R.array.wiimoteExtensionsEntries,
              R.array.wiimoteExtensionsValues, 0, extension,
              MenuTag.getWiimoteExtensionMenuTag(wiimoteNumber)));
    }

    sl.add(new HeaderSetting(null, null, R.string.generic_buttons, 0));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_A + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.button_a, bindA, mGameID));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_B + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.button_b, bindB, mGameID));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_1 + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.button_one, bind1, mGameID));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_2 + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.button_two, bind2, mGameID));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_MINUS + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.button_minus, bindMinus, mGameID));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_PLUS + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.button_plus, bindPlus, mGameID));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_HOME + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.button_home, bindHome, mGameID));

    sl.add(new HeaderSetting(null, null, R.string.wiimote_ir, 0));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_IR_UP + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.generic_up, bindIRUp, mGameID));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_IR_DOWN + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.generic_down, bindIRDown, mGameID));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_IR_LEFT + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.generic_left, bindIRLeft, mGameID));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_IR_RIGHT + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.generic_right, bindIRRight, mGameID));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_IR_FORWARD + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.generic_forward, bindIRForward, mGameID));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_IR_BACKWARD + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.generic_backward, bindIRBackward, mGameID));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_IR_HIDE + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.ir_hide, bindIRHide, mGameID));

    sl.add(new HeaderSetting(null, null, R.string.wiimote_swing, 0));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_SWING_UP + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.generic_up, bindSwingUp, mGameID));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_SWING_DOWN + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.generic_down, bindSwingDown, mGameID));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_SWING_LEFT + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.generic_left, bindSwingLeft, mGameID));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_SWING_RIGHT + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.generic_right, bindSwingRight, mGameID));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_SWING_FORWARD + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.generic_forward, bindSwingForward, mGameID));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_SWING_BACKWARD + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.generic_backward, bindSwingBackward, mGameID));

    sl.add(new HeaderSetting(null, null, R.string.wiimote_tilt, 0));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TILT_FORWARD + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.generic_forward, bindTiltForward, mGameID));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TILT_BACKWARD + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.generic_backward, bindTiltBackward, mGameID));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TILT_LEFT + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.generic_left, bindTiltLeft, mGameID));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TILT_RIGHT + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.generic_right, bindTiltRight, mGameID));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TILT_MODIFIER + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.tilt_modifier, bindTiltModifier, mGameID));

    sl.add(new HeaderSetting(null, null, R.string.wiimote_shake, 0));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_SHAKE_X + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.shake_x, bindShakeX, mGameID));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_SHAKE_Y + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.shake_y, bindShakeY, mGameID));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_SHAKE_Z + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.shake_z, bindShakeZ, mGameID));

    sl.add(new HeaderSetting(null, null, R.string.controller_dpad, 0));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DPAD_UP + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.generic_up, bindDPadUp, mGameID));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DPAD_DOWN + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.generic_down, bindDPadDown, mGameID));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DPAD_LEFT + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.generic_left, bindDPadLeft, mGameID));
    sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DPAD_RIGHT + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.generic_right, bindDPadRight, mGameID));


    sl.add(new HeaderSetting(null, null, R.string.emulation_control_rumble, 0));
    sl.add(new RumbleBindingSetting(SettingsFile.KEY_EMU_RUMBLE + wiimoteNumber,
            Settings.SECTION_BINDINGS, R.string.emulation_control_rumble, wiiEmuRumble, mGameID));
  }

  private void addExtensionTypeSettings(ArrayList<SettingsItem> sl, int wiimoteNumber,
          int extentionType)
  {
    SettingSection bindingsSection = mSettings.getSection(Settings.SECTION_BINDINGS);

    switch (extentionType)
    {
      case 1: // Nunchuk
        Setting bindC =
                bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_C + wiimoteNumber);
        Setting bindZ =
                bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_Z + wiimoteNumber);
        Setting bindUp =
                bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_UP + wiimoteNumber);
        Setting bindDown =
                bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_DOWN + wiimoteNumber);
        Setting bindLeft =
                bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_LEFT + wiimoteNumber);
        Setting bindRight =
                bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_RIGHT + wiimoteNumber);
        Setting bindSwingUp = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_UP + wiimoteNumber);
        Setting bindSwingDown = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_DOWN + wiimoteNumber);
        Setting bindSwingLeft = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_LEFT + wiimoteNumber);
        Setting bindSwingRight = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_RIGHT + wiimoteNumber);
        Setting bindSwingForward = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_FORWARD + wiimoteNumber);
        Setting bindSwingBackward = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_BACKWARD + wiimoteNumber);
        Setting bindTiltForward = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_FORWARD + wiimoteNumber);
        Setting bindTiltBackward = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_BACKWARD + wiimoteNumber);
        Setting bindTiltLeft = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_LEFT + wiimoteNumber);
        Setting bindTiltRight = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_RIGHT + wiimoteNumber);
        Setting bindTiltModifier = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_MODIFIER + wiimoteNumber);
        Setting bindShakeX = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SHAKE_X + wiimoteNumber);
        Setting bindShakeY = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SHAKE_Y + wiimoteNumber);
        Setting bindShakeZ = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SHAKE_Z + wiimoteNumber);

        sl.add(new HeaderSetting(null, null, R.string.generic_buttons, 0));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_C + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.nunchuk_button_c, bindC, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_Z + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.button_z, bindZ, mGameID));

        sl.add(new HeaderSetting(null, null, R.string.generic_stick, 0));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_UP + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_up, bindUp, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_DOWN + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_down, bindDown, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_LEFT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_left, bindLeft, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_RIGHT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_right, bindRight, mGameID));

        sl.add(new HeaderSetting(null, null, R.string.wiimote_swing, 0));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_UP + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_up, bindSwingUp, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_DOWN + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_down, bindSwingDown, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_LEFT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_left, bindSwingLeft, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_RIGHT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_right, bindSwingRight, mGameID));
        sl.add(new InputBindingSetting(
                SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_FORWARD + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_forward, bindSwingForward, mGameID));
        sl.add(new InputBindingSetting(
                SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_BACKWARD + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_backward, bindSwingBackward, mGameID));

        sl.add(new HeaderSetting(null, null, R.string.wiimote_tilt, 0));
        sl.add(new InputBindingSetting(
                SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_FORWARD + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_forward, bindTiltForward, mGameID));
        sl.add(new InputBindingSetting(
                SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_BACKWARD + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_backward, bindTiltBackward, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_LEFT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_left, bindTiltLeft, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_RIGHT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_right, bindTiltRight, mGameID));
        sl.add(new InputBindingSetting(
                SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_MODIFIER + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.tilt_modifier, bindTiltModifier, mGameID));

        sl.add(new HeaderSetting(null, null, R.string.wiimote_shake, 0));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SHAKE_X + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.shake_x, bindShakeX, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SHAKE_Y + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.shake_y, bindShakeY, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SHAKE_Z + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.shake_z, bindShakeZ, mGameID));
        break;
      case 2: // Classic
        Setting bindA =
                bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_A + wiimoteNumber);
        Setting bindB =
                bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_B + wiimoteNumber);
        Setting bindX =
                bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_X + wiimoteNumber);
        Setting bindY =
                bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_Y + wiimoteNumber);
        Setting bindZL =
                bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_ZL + wiimoteNumber);
        Setting bindZR =
                bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_ZR + wiimoteNumber);
        Setting bindMinus =
                bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_MINUS + wiimoteNumber);
        Setting bindPlus =
                bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_PLUS + wiimoteNumber);
        Setting bindHome =
                bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_HOME + wiimoteNumber);
        Setting bindLeftUp = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_UP + wiimoteNumber);
        Setting bindLeftDown = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_DOWN + wiimoteNumber);
        Setting bindLeftLeft = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_LEFT + wiimoteNumber);
        Setting bindLeftRight = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_RIGHT + wiimoteNumber);
        Setting bindRightUp = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_UP + wiimoteNumber);
        Setting bindRightDown = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_DOWN + wiimoteNumber);
        Setting bindRightLeft = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_LEFT + wiimoteNumber);
        Setting bindRightRight = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_RIGHT + wiimoteNumber);
        Setting bindL = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_TRIGGER_L + wiimoteNumber);
        Setting bindR = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_TRIGGER_R + wiimoteNumber);
        Setting bindDpadUp = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_UP + wiimoteNumber);
        Setting bindDpadDown = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_DOWN + wiimoteNumber);
        Setting bindDpadLeft = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_LEFT + wiimoteNumber);
        Setting bindDpadRight = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_RIGHT + wiimoteNumber);

        sl.add(new HeaderSetting(null, null, R.string.generic_buttons, 0));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_A + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.button_a, bindA, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_B + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.button_b, bindB, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_X + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.button_x, bindX, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_Y + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.button_y, bindY, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_ZL + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.classic_button_zl, bindZL, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_ZR + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.classic_button_zr, bindZR, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_MINUS + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.button_minus, bindMinus, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_PLUS + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.button_plus, bindPlus, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_HOME + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.button_home, bindHome, mGameID));

        sl.add(new HeaderSetting(null, null, R.string.classic_leftstick, 0));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_UP + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_up, bindLeftUp, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_DOWN + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_down, bindLeftDown, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_LEFT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_left, bindLeftLeft, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_RIGHT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_right, bindLeftRight, mGameID));

        sl.add(new HeaderSetting(null, null, R.string.classic_rightstick, 0));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_UP + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_up, bindRightUp, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_DOWN + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_down, bindRightDown, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_LEFT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_left, bindRightLeft, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_RIGHT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_right, bindRightRight, mGameID));

        sl.add(new HeaderSetting(null, null, R.string.controller_trig, 0));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_TRIGGER_L + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.trigger_left, bindR, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_TRIGGER_R + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.trigger_right, bindL, mGameID));

        sl.add(new HeaderSetting(null, null, R.string.controller_dpad, 0));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_UP + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_up, bindDpadUp, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_DOWN + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_down, bindDpadDown, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_LEFT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_left, bindDpadLeft, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_RIGHT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_right, bindDpadRight, mGameID));
        break;
      case 3: // Guitar
        Setting bindFretGreen = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_GREEN + wiimoteNumber);
        Setting bindFretRed = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_RED + wiimoteNumber);
        Setting bindFretYellow = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_YELLOW + wiimoteNumber);
        Setting bindFretBlue = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_BLUE + wiimoteNumber);
        Setting bindFretOrange = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_ORANGE + wiimoteNumber);
        Setting bindStrumUp = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_GUITAR_STRUM_UP + wiimoteNumber);
        Setting bindStrumDown = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_GUITAR_STRUM_DOWN + wiimoteNumber);
        Setting bindGuitarMinus =
                bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_GUITAR_MINUS + wiimoteNumber);
        Setting bindGuitarPlus =
                bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_GUITAR_PLUS + wiimoteNumber);
        Setting bindGuitarUp = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_GUITAR_STICK_UP + wiimoteNumber);
        Setting bindGuitarDown = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_GUITAR_STICK_DOWN + wiimoteNumber);
        Setting bindGuitarLeft = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_GUITAR_STICK_LEFT + wiimoteNumber);
        Setting bindGuitarRight = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_GUITAR_STICK_RIGHT + wiimoteNumber);
        Setting bindWhammyBar = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_GUITAR_WHAMMY_BAR + wiimoteNumber);

        sl.add(new HeaderSetting(null, null, R.string.guitar_frets, 0));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_GREEN + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_green, bindFretGreen, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_RED + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_red, bindFretRed, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_YELLOW + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_yellow, bindFretYellow, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_BLUE + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_blue, bindFretBlue, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_ORANGE + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_orange, bindFretOrange, mGameID));

        sl.add(new HeaderSetting(null, null, R.string.guitar_strum, 0));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_STRUM_UP + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_up, bindStrumUp, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_STRUM_DOWN + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_down, bindStrumDown, mGameID));

        sl.add(new HeaderSetting(null, null, R.string.generic_buttons, 0));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_MINUS + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.button_minus, bindGuitarMinus, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_PLUS + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.button_plus, bindGuitarPlus, mGameID));

        sl.add(new HeaderSetting(null, null, R.string.generic_stick, 0));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_STICK_UP + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_up, bindGuitarUp, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_STICK_DOWN + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_down, bindGuitarDown, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_STICK_LEFT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_left, bindGuitarLeft, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_STICK_RIGHT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_right, bindGuitarRight, mGameID));

        sl.add(new HeaderSetting(null, null, R.string.guitar_whammy, 0));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_WHAMMY_BAR + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_right, bindWhammyBar, mGameID));
        break;
      case 4: // Drums
        Setting bindPadRed =
                bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_RED + wiimoteNumber);
        Setting bindPadYellow = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_YELLOW + wiimoteNumber);
        Setting bindPadBlue =
                bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_BLUE + wiimoteNumber);
        Setting bindPadGreen = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_GREEN + wiimoteNumber);
        Setting bindPadOrange = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_ORANGE + wiimoteNumber);
        Setting bindPadBass =
                bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_BASS + wiimoteNumber);
        Setting bindDrumsUp =
                bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_DRUMS_STICK_UP + wiimoteNumber);
        Setting bindDrumsDown = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_DRUMS_STICK_DOWN + wiimoteNumber);
        Setting bindDrumsLeft = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_DRUMS_STICK_LEFT + wiimoteNumber);
        Setting bindDrumsRight = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_DRUMS_STICK_RIGHT + wiimoteNumber);
        Setting bindDrumsMinus =
                bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_DRUMS_MINUS + wiimoteNumber);
        Setting bindDrumsPlus =
                bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_DRUMS_PLUS + wiimoteNumber);

        sl.add(new HeaderSetting(null, null, R.string.drums_pads, 0));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_RED + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_red, bindPadRed, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_YELLOW + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_yellow, bindPadYellow, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_BLUE + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_blue, bindPadBlue, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_GREEN + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_green, bindPadGreen, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_ORANGE + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_orange, bindPadOrange, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_BASS + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.drums_pad_bass, bindPadBass, mGameID));

        sl.add(new HeaderSetting(null, null, R.string.generic_stick, 0));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_STICK_UP + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_up, bindDrumsUp, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_STICK_DOWN + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_down, bindDrumsDown, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_STICK_LEFT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_left, bindDrumsLeft, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_STICK_RIGHT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_right, bindDrumsRight, mGameID));

        sl.add(new HeaderSetting(null, null, R.string.generic_buttons, 0));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_MINUS + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.button_minus, bindDrumsMinus, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_PLUS + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.button_plus, bindDrumsPlus, mGameID));
        break;
      case 5: // Turntable
        Setting bindGreenLeft = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_GREEN_LEFT + wiimoteNumber);
        Setting bindRedLeft = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_RED_LEFT + wiimoteNumber);
        Setting bindBlueLeft = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_BLUE_LEFT + wiimoteNumber);
        Setting bindGreenRight = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_GREEN_RIGHT + wiimoteNumber);
        Setting bindRedRight = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_RED_RIGHT + wiimoteNumber);
        Setting bindBlueRight = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_BLUE_RIGHT + wiimoteNumber);
        Setting bindTurntableMinus = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_MINUS + wiimoteNumber);
        Setting bindTurntablePlus =
                bindingsSection.getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_PLUS + wiimoteNumber);
        Setting bindEuphoria = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_EUPHORIA + wiimoteNumber);
        Setting bindTurntableLeftLeft = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_LEFT_LEFT + wiimoteNumber);
        Setting bindTurntableLeftRight = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_LEFT_RIGHT + wiimoteNumber);
        Setting bindTurntableRightLeft = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_RIGHT_LEFT + wiimoteNumber);
        Setting bindTurntableRightRight = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_RIGHT_RIGHT + wiimoteNumber);
        Setting bindTurntableUp = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_STICK_UP + wiimoteNumber);
        Setting bindTurntableDown = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_STICK_DOWN + wiimoteNumber);
        Setting bindTurntableLeft = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_STICK_LEFT + wiimoteNumber);
        Setting bindTurntableRight = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_STICK_RIGHT + wiimoteNumber);
        Setting bindEffectDial = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_EFFECT_DIAL + wiimoteNumber);
        Setting bindCrossfadeLeft = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_CROSSFADE_LEFT + wiimoteNumber);
        Setting bindCrossfadeRight = bindingsSection
                .getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_CROSSFADE_RIGHT + wiimoteNumber);

        sl.add(new HeaderSetting(null, null, R.string.generic_buttons, 0));
        sl.add(new InputBindingSetting(
                SettingsFile.KEY_WIIBIND_TURNTABLE_GREEN_LEFT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.turntable_button_green_left, bindGreenLeft,
                mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_RED_LEFT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.turntable_button_red_left, bindRedLeft,
                mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_BLUE_LEFT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.turntable_button_blue_left, bindBlueLeft,
                mGameID));
        sl.add(new InputBindingSetting(
                SettingsFile.KEY_WIIBIND_TURNTABLE_GREEN_RIGHT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.turntable_button_green_right, bindGreenRight,
                mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_RED_RIGHT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.turntable_button_red_right, bindRedRight,
                mGameID));
        sl.add(new InputBindingSetting(
                SettingsFile.KEY_WIIBIND_TURNTABLE_BLUE_RIGHT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.turntable_button_blue_right, bindBlueRight,
                mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_MINUS + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.button_minus, bindTurntableMinus, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_PLUS + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.button_plus, bindTurntablePlus, mGameID));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_EUPHORIA + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.turntable_button_euphoria, bindEuphoria,
                mGameID));

        sl.add(new HeaderSetting(null, null, R.string.turntable_table_left, 0));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_LEFT_LEFT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_left, bindTurntableLeftLeft, mGameID));
        sl.add(new InputBindingSetting(
                SettingsFile.KEY_WIIBIND_TURNTABLE_LEFT_RIGHT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_right, bindTurntableLeftRight,
                mGameID));

        sl.add(new HeaderSetting(null, null, R.string.turntable_table_right, 0));
        sl.add(new InputBindingSetting(
                SettingsFile.KEY_WIIBIND_TURNTABLE_RIGHT_LEFT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_left, bindTurntableRightLeft, mGameID));
        sl.add(new InputBindingSetting(
                SettingsFile.KEY_WIIBIND_TURNTABLE_RIGHT_RIGHT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_right, bindTurntableRightRight,
                mGameID));

        sl.add(new HeaderSetting(null, null, R.string.generic_stick, 0));
        sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_STICK_UP + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_up, bindTurntableUp, mGameID));
        sl.add(new InputBindingSetting(
                SettingsFile.KEY_WIIBIND_TURNTABLE_STICK_DOWN + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_down, bindTurntableDown, mGameID));
        sl.add(new InputBindingSetting(
                SettingsFile.KEY_WIIBIND_TURNTABLE_STICK_LEFT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_left, bindTurntableLeft, mGameID));
        sl.add(new InputBindingSetting(
                SettingsFile.KEY_WIIBIND_TURNTABLE_STICK_RIGHT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_right, bindTurntableRight, mGameID));

        sl.add(new HeaderSetting(null, null, R.string.turntable_effect, 0));
        sl.add(new InputBindingSetting(
                SettingsFile.KEY_WIIBIND_TURNTABLE_EFFECT_DIAL + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.turntable_effect_dial, bindEffectDial,
                mGameID));

        sl.add(new HeaderSetting(null, null, R.string.turntable_crossfade, 0));
        sl.add(new InputBindingSetting(
                SettingsFile.KEY_WIIBIND_TURNTABLE_CROSSFADE_LEFT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_left, bindCrossfadeLeft, mGameID));
        sl.add(new InputBindingSetting(
                SettingsFile.KEY_WIIBIND_TURNTABLE_CROSSFADE_RIGHT + wiimoteNumber,
                Settings.SECTION_BINDINGS, R.string.generic_right, bindCrossfadeRight, mGameID));
        break;
    }
  }

  private boolean getInvertedBooleanValue(String section, String key, boolean defaultValue)
  {
    try
    {
      return !((BooleanSetting) mSettings.getSection(section).getSetting(key)).getValue();
    }
    catch (NullPointerException ex)
    {
      return defaultValue;
    }
  }

  private int getVideoBackendValue()
  {
    SettingSection coreSection = mSettings.getSection(Settings.SECTION_INI_CORE);

    int videoBackendValue;

    try
    {
      String videoBackend =
              ((StringSetting) coreSection.getSetting(SettingsFile.KEY_VIDEO_BACKEND)).getValue();
      if (videoBackend.equals("OGL"))
      {
        videoBackendValue = 0;
      }
      else if (videoBackend.equals("Vulkan"))
      {
        videoBackendValue = 1;
      }
      else if (videoBackend.equals("Software Renderer"))
      {
        videoBackendValue = 2;
      }
      else if (videoBackend.equals("Null"))
      {
        videoBackendValue = 3;
      }
      else
      {
        videoBackendValue = 0;
      }
    }
    catch (NullPointerException ex)
    {
      videoBackendValue = 0;
    }

    return videoBackendValue;
  }

  private int getExtensionValue(int wiimoteNumber)
  {
    int extensionValue;

    try
    {
      String extension;
      if (mGameID.equals("")) // Main settings
      {
        extension =
                ((StringSetting) mSettings.getSection(Settings.SECTION_WIIMOTE + wiimoteNumber)
                        .getSetting(SettingsFile.KEY_WIIMOTE_EXTENSION)).getValue();
      }
      else // Game settings
      {
        extension = ((StringSetting) mSettings.getSection(Settings.SECTION_PROFILE)
                .getSetting(SettingsFile.KEY_WIIMOTE_EXTENSION)).getValue();
      }

      if (extension.equals("None"))
      {
        extensionValue = 0;
      }
      else if (extension.equals("Nunchuk"))
      {
        extensionValue = 1;
      }
      else if (extension.equals("Classic"))
      {
        extensionValue = 2;
      }
      else if (extension.equals("Guitar"))
      {
        extensionValue = 3;
      }
      else if (extension.equals("Drums"))
      {
        extensionValue = 4;
      }
      else if (extension.equals("Turntable"))
      {
        extensionValue = 5;
      }
      else
      {
        extensionValue = 0;
      }
    }
    catch (NullPointerException ex)
    {
      extensionValue = 0;
    }

    return extensionValue;
  }
}
