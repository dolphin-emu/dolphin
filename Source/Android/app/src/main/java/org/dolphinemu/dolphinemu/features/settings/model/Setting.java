package org.dolphinemu.dolphinemu.features.settings.model;

public enum Setting implements StringSetting, BooleanSetting, IntSetting, FloatSetting
{
  // These entries have the same names and order as in C++, just for consistency.

  MAIN_CPU_CORE(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "CPUCore"),
  MAIN_DSP_HLE(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "DSPHLE"),
  MAIN_CPU_THREAD(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "CPUThread"),
  MAIN_DEFAULT_ISO(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "DefaultISO"),
  MAIN_GC_LANGUAGE(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "SelectedLanguage"),
  MAIN_OVERRIDE_REGION_SETTINGS(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE,
          "OverrideRegionSettings"),
  MAIN_AUDIO_STRETCH(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "AudioStretch"),
  MAIN_SLOT_A(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "SlotA"),
  MAIN_SLOT_B(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "SlotB"),
  MAIN_WII_SD_CARD(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "WiiSDCard"),
  MAIN_WIIMOTE_CONTINUOUS_SCANNING(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE,
          "WiimoteContinuousScanning"),
  MAIN_WIIMOTE_ENABLE_SPEAKER(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE,
          "WiimoteEnableSpeaker"),
  MAIN_EMULATION_SPEED(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "EmulationSpeed"),
  MAIN_OVERCLOCK(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "Overclock"),
  MAIN_OVERCLOCK_ENABLE(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "OverclockEnable"),
  MAIN_GFX_BACKEND(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "GFXBackend"),
  MAIN_AUTO_DISC_CHANGE(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "AutoDiscChange"),
  MAIN_ALLOW_SD_WRITES(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "WiiSDCardAllowWrites"),
  MAIN_ENABLE_SAVESTATES(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "EnableSaveStates"),

  MAIN_DSP_JIT(Settings.FILE_DOLPHIN, Settings.SECTION_INI_DSP, "EnableJIT"),
  MAIN_AUDIO_VOLUME(Settings.FILE_DOLPHIN, Settings.SECTION_INI_DSP, "Volume"),

  MAIN_DUMP_PATH(Settings.FILE_DOLPHIN, Settings.SECTION_INI_GENERAL, "DumpPath"),
  MAIN_LOAD_PATH(Settings.FILE_DOLPHIN, Settings.SECTION_INI_GENERAL, "LoadPath"),
  MAIN_RESOURCEPACK_PATH(Settings.FILE_DOLPHIN, Settings.SECTION_INI_GENERAL, "ResourcePackPath"),
  MAIN_FS_PATH(Settings.FILE_DOLPHIN, Settings.SECTION_INI_GENERAL, "NANDRootPath"),
  MAIN_SD_PATH(Settings.FILE_DOLPHIN, Settings.SECTION_INI_GENERAL, "WiiSDCardPath"),

  MAIN_USE_PANIC_HANDLERS(Settings.FILE_DOLPHIN, Settings.SECTION_INI_INTERFACE,
          "UsePanicHandlers"),
  MAIN_OSD_MESSAGES(Settings.FILE_DOLPHIN, Settings.SECTION_INI_INTERFACE,
          "OnScreenDisplayMessages"),

  MAIN_ANALYTICS_ENABLED(Settings.FILE_DOLPHIN, Settings.SECTION_ANALYTICS, "Enabled"),
  MAIN_ANALYTICS_PERMISSION_ASKED(Settings.FILE_DOLPHIN, Settings.SECTION_ANALYTICS,
          "PermissionAsked"),

  MAIN_RECURSIVE_ISO_PATHS(Settings.FILE_DOLPHIN, Settings.SECTION_INI_GENERAL,
          "RecursiveISOPaths"),

  MAIN_LAST_PLATFORM_TAB(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID, "LastPlatformTab"),

  GFX_WIDESCREEN_HACK(Settings.FILE_GFX, Settings.SECTION_GFX_SETTINGS, "wideScreenHack"),
  GFX_ASPECT_RATIO(Settings.FILE_GFX, Settings.SECTION_GFX_SETTINGS, "AspectRatio"),
  GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES(Settings.FILE_GFX, Settings.SECTION_GFX_SETTINGS,
          "SafeTextureCacheColorSamples"),
  GFX_SHOW_FPS(Settings.FILE_GFX, Settings.SECTION_GFX_SETTINGS, "ShowFPS"),
  GFX_ENABLE_GPU_TEXTURE_DECODING(Settings.FILE_GFX, Settings.SECTION_GFX_SETTINGS,
          "EnableGPUTextureDecoding"),
  GFX_ENABLE_PIXEL_LIGHTING(Settings.FILE_GFX, Settings.SECTION_GFX_SETTINGS,
          "EnablePixelLighting"),
  GFX_FAST_DEPTH_CALC(Settings.FILE_GFX, Settings.SECTION_GFX_SETTINGS, "FastDepthCalc"),
  GFX_MSAA(Settings.FILE_GFX, Settings.SECTION_GFX_SETTINGS, "MSAA"),
  GFX_EFB_SCALE(Settings.FILE_GFX, Settings.SECTION_GFX_SETTINGS, "InternalResolution"),
  GFX_DISABLE_FOG(Settings.FILE_GFX, Settings.SECTION_GFX_SETTINGS, "DisableFog"),
  GFX_BACKEND_MULTITHREADING(Settings.FILE_GFX, Settings.SECTION_GFX_SETTINGS,
          "BackendMultithreading"),
  GFX_WAIT_FOR_SHADERS_BEFORE_STARTING(Settings.FILE_GFX, Settings.SECTION_GFX_SETTINGS,
          "WaitForShadersBeforeStarting"),
  GFX_SHADER_COMPILATION_MODE(Settings.FILE_GFX, Settings.SECTION_GFX_SETTINGS,
          "ShaderCompilationMode"),

  GFX_ENHANCE_FORCE_FILTERING(Settings.FILE_GFX, Settings.SECTION_GFX_ENHANCEMENTS,
          "ForceFiltering"),
  GFX_ENHANCE_MAX_ANISOTROPY(Settings.FILE_GFX, Settings.SECTION_GFX_ENHANCEMENTS, "MaxAnisotropy"),
  GFX_ENHANCE_POST_SHADER(Settings.FILE_GFX, Settings.SECTION_GFX_ENHANCEMENTS,
          "PostProcessingShader"),
  GFX_ENHANCE_FORCE_TRUE_COLOR(Settings.FILE_GFX, Settings.SECTION_GFX_ENHANCEMENTS,
          "ForceTrueColor"),
  GFX_ENHANCE_DISABLE_COPY_FILTER(Settings.FILE_GFX, Settings.SECTION_GFX_ENHANCEMENTS,
          "DisableCopyFilter"),
  GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION(Settings.FILE_GFX, Settings.SECTION_GFX_ENHANCEMENTS,
          "ArbitraryMipmapDetection"),

  GFX_STEREO_MODE(Settings.FILE_GFX, Settings.SECTION_STEREOSCOPY, "StereoMode"),
  GFX_STEREO_DEPTH(Settings.FILE_GFX, Settings.SECTION_STEREOSCOPY, "StereoDepth"),
  GFX_STEREO_CONVERGENCE_PERCENTAGE(Settings.FILE_GFX, Settings.SECTION_STEREOSCOPY,
          "StereoConvergencePercentage"),
  GFX_STEREO_SWAP_EYES(Settings.FILE_GFX, Settings.SECTION_STEREOSCOPY, "StereoSwapEyes"),

  GFX_HACK_EFB_ACCESS_ENABLE(Settings.FILE_GFX, Settings.SECTION_GFX_HACKS, "EFBAccessEnable"),
  GFX_HACK_SKIP_EFB_COPY_TO_RAM(Settings.FILE_GFX, Settings.SECTION_GFX_HACKS,
          "EFBToTextureEnable"),
  GFX_HACK_SKIP_XFB_COPY_TO_RAM(Settings.FILE_GFX, Settings.SECTION_GFX_HACKS,
          "XFBToTextureEnable"),
  GFX_HACK_DEFER_EFB_COPIES(Settings.FILE_GFX, Settings.SECTION_GFX_HACKS, "DeferEFBCopies"),
  GFX_HACK_IMMEDIATE_XFB(Settings.FILE_GFX, Settings.SECTION_GFX_HACKS, "ImmediateXFBEnable"),
  GFX_HACK_SKIP_DUPLICATE_XFBS(Settings.FILE_GFX, Settings.SECTION_GFX_HACKS, "SkipDuplicateXFBs"),
  GFX_HACK_COPY_EFB_SCALED(Settings.FILE_GFX, Settings.SECTION_GFX_HACKS, "EFBScaledCopy"),
  GFX_HACK_EFB_EMULATE_FORMAT_CHANGES(Settings.FILE_GFX, Settings.SECTION_GFX_HACKS,
          "EFBEmulateFormatChanges"),

  LOGGER_WRITE_TO_FILE(Settings.FILE_LOGGER, Settings.SECTION_LOGGER_OPTIONS, "WriteToFile"),
  LOGGER_VERBOSITY(Settings.FILE_LOGGER, Settings.SECTION_LOGGER_OPTIONS, "Verbosity"),

  // These settings are not yet in the new config system in C++ - please move them once they are

  MAIN_JIT_OFF(Settings.FILE_DOLPHIN, Settings.SECTION_DEBUG, "JitOff"),
  MAIN_JIT_LOAD_STORE_OFF(Settings.FILE_DOLPHIN, Settings.SECTION_DEBUG, "JitLoadStoreOff"),
  MAIN_JIT_LOAD_STORE_FLOATING_OFF(Settings.FILE_DOLPHIN, Settings.SECTION_DEBUG,
          "JitLoadStoreFloatingOff"),
  MAIN_JIT_LOAD_STORE_PAIRED_OFF(Settings.FILE_DOLPHIN, Settings.SECTION_DEBUG,
          "JitLoadStorePairedOff"),
  MAIN_JIT_FLOATING_POINT_OFF(Settings.FILE_DOLPHIN, Settings.SECTION_DEBUG, "JitFloatingPointOff"),
  MAIN_JIT_INTEGER_OFF(Settings.FILE_DOLPHIN, Settings.SECTION_DEBUG, "JitIntegerOff"),
  MAIN_JIT_PAIRED_OFF(Settings.FILE_DOLPHIN, Settings.SECTION_DEBUG, "JitPairedOff"),
  MAIN_JIT_SYSTEM_REGISTERS_OFF(Settings.FILE_DOLPHIN, Settings.SECTION_DEBUG,
          "JitSystemRegistersOff"),
  MAIN_JIT_BRANCH_OFF(Settings.FILE_DOLPHIN, Settings.SECTION_DEBUG, "JitBranchOff"),
  MAIN_JIT_REGISTER_CACHE_OFF(Settings.FILE_DOLPHIN, Settings.SECTION_DEBUG, "JitRegisterCacheOff");

  private final String mFile;
  private final String mSection;
  private final String mKey;

  Setting(String file, String section, String key)
  {
    mFile = file;
    mSection = section;
    mKey = key;
  }

  @Override
  public boolean delete(Settings settings)
  {
    return settings.getSection(mFile, mSection).delete(mKey);
  }

  @Override
  public String getString(Settings settings, String defaultValue)
  {
    return settings.getSection(mFile, mSection).getString(mKey, defaultValue);
  }

  @Override
  public boolean getBoolean(Settings settings, boolean defaultValue)
  {
    return settings.getSection(mFile, mSection).getBoolean(mKey, defaultValue);
  }

  @Override
  public int getInt(Settings settings, int defaultValue)
  {
    return settings.getSection(mFile, mSection).getInt(mKey, defaultValue);
  }

  @Override
  public float getFloat(Settings settings, float defaultValue)
  {
    return settings.getSection(mFile, mSection).getFloat(mKey, defaultValue);
  }

  @Override
  public void setString(Settings settings, String newValue)
  {
    settings.getSection(mFile, mSection).setString(mKey, newValue);
  }

  @Override
  public void setBoolean(Settings settings, boolean newValue)
  {
    settings.getSection(mFile, mSection).setBoolean(mKey, newValue);
  }

  @Override
  public void setInt(Settings settings, int newValue)
  {
    settings.getSection(mFile, mSection).setInt(mKey, newValue);
  }

  @Override
  public void setFloat(Settings settings, float newValue)
  {
    settings.getSection(mFile, mSection).setFloat(mKey, newValue);
  }
}
