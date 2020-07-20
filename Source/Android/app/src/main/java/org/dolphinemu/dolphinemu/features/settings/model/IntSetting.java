package org.dolphinemu.dolphinemu.features.settings.model;

import org.dolphinemu.dolphinemu.NativeLibrary;

public enum IntSetting implements AbstractIntSetting
{
  // These entries have the same names and order as in C++, just for consistency.

  MAIN_CPU_CORE(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "CPUCore",
          NativeLibrary.DefaultCPUCore()),
  MAIN_GC_LANGUAGE(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "SelectedLanguage", 0),
  MAIN_SLOT_A(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "SlotA", 8),
  MAIN_SLOT_B(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "SlotB", 255),

  MAIN_AUDIO_VOLUME(Settings.FILE_DOLPHIN, Settings.SECTION_INI_DSP, "Volume", 100),

  MAIN_LAST_PLATFORM_TAB(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID, "LastPlatformTab", 0),

  GFX_ASPECT_RATIO(Settings.FILE_GFX, Settings.SECTION_GFX_SETTINGS, "AspectRatio", 0),
  GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES(Settings.FILE_GFX, Settings.SECTION_GFX_SETTINGS,
          "SafeTextureCacheColorSamples", 128),
  GFX_MSAA(Settings.FILE_GFX, Settings.SECTION_GFX_SETTINGS, "MSAA", 1),
  GFX_EFB_SCALE(Settings.FILE_GFX, Settings.SECTION_GFX_SETTINGS, "InternalResolution", 1),
  GFX_SHADER_COMPILATION_MODE(Settings.FILE_GFX, Settings.SECTION_GFX_SETTINGS,
          "ShaderCompilationMode", 0),

  GFX_ENHANCE_MAX_ANISOTROPY(Settings.FILE_GFX, Settings.SECTION_GFX_ENHANCEMENTS, "MaxAnisotropy",
          0),

  GFX_STEREO_MODE(Settings.FILE_GFX, Settings.SECTION_STEREOSCOPY, "StereoMode", 0),
  GFX_STEREO_DEPTH(Settings.FILE_GFX, Settings.SECTION_STEREOSCOPY, "StereoDepth", 20),
  GFX_STEREO_CONVERGENCE_PERCENTAGE(Settings.FILE_GFX, Settings.SECTION_STEREOSCOPY,
          "StereoConvergencePercentage", 100),

  LOGGER_VERBOSITY(Settings.FILE_LOGGER, Settings.SECTION_LOGGER_OPTIONS, "Verbosity", 1);

  private final String mFile;
  private final String mSection;
  private final String mKey;
  private final int mDefaultValue;

  IntSetting(String file, String section, String key, int defaultValue)
  {
    mFile = file;
    mSection = section;
    mKey = key;
    mDefaultValue = defaultValue;
  }

  @Override
  public boolean delete(Settings settings)
  {
    return settings.getSection(mFile, mSection).delete(mKey);
  }

  @Override
  public int getInt(Settings settings)
  {
    return settings.getSection(mFile, mSection).getInt(mKey, mDefaultValue);
  }

  @Override
  public void setInt(Settings settings, int newValue)
  {
    settings.getSection(mFile, mSection).setInt(mKey, newValue);
  }
}
