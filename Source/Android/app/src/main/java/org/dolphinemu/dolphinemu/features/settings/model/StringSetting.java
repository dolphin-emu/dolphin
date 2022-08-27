// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model;

import org.dolphinemu.dolphinemu.NativeLibrary;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

public enum StringSetting implements AbstractStringSetting
{
  // These entries have the same names and order as in C++, just for consistency.

  MAIN_DEFAULT_ISO(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "DefaultISO", ""),

  MAIN_BBA_MAC(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "BBA_MAC", ""),
  MAIN_BBA_XLINK_IP(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "BBA_XLINK_IP", ""),

  // Schthack PSO Server - https://schtserv.com/
  MAIN_BBA_BUILTIN_DNS(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "BBA_BUILTIN_DNS",
          "149.56.167.128"),

  MAIN_GFX_BACKEND(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "GFXBackend",
          NativeLibrary.GetDefaultGraphicsBackendName()),

  MAIN_DUMP_PATH(Settings.FILE_DOLPHIN, Settings.SECTION_INI_GENERAL, "DumpPath", ""),
  MAIN_LOAD_PATH(Settings.FILE_DOLPHIN, Settings.SECTION_INI_GENERAL, "LoadPath", ""),
  MAIN_RESOURCEPACK_PATH(Settings.FILE_DOLPHIN, Settings.SECTION_INI_GENERAL, "ResourcePackPath",
          ""),
  MAIN_FS_PATH(Settings.FILE_DOLPHIN, Settings.SECTION_INI_GENERAL, "NANDRootPath", ""),
  MAIN_WII_SD_CARD_IMAGE_PATH(Settings.FILE_DOLPHIN, Settings.SECTION_INI_GENERAL, "WiiSDCardPath",
          ""),
  MAIN_WII_SD_CARD_SYNC_FOLDER_PATH(Settings.FILE_DOLPHIN, Settings.SECTION_INI_GENERAL,
          "WiiSDCardSyncFolder", ""),
  MAIN_WFS_PATH(Settings.FILE_DOLPHIN, Settings.SECTION_INI_GENERAL, "WFSPath", ""),

  GFX_ENHANCE_POST_SHADER(Settings.FILE_GFX, Settings.SECTION_GFX_ENHANCEMENTS,
          "PostProcessingShader", "");

  private static final StringSetting[] NOT_RUNTIME_EDITABLE_ARRAY = new StringSetting[]{
          MAIN_GFX_BACKEND,
  };

  private static final Set<StringSetting> NOT_RUNTIME_EDITABLE =
          new HashSet<>(Arrays.asList(NOT_RUNTIME_EDITABLE_ARRAY));

  private final String mFile;
  private final String mSection;
  private final String mKey;
  private final String mDefaultValue;

  StringSetting(String file, String section, String key, String defaultValue)
  {
    mFile = file;
    mSection = section;
    mKey = key;
    mDefaultValue = defaultValue;
  }

  @Override
  public boolean isOverridden(Settings settings)
  {
    if (settings.isGameSpecific() && !NativeConfig.isSettingSaveable(mFile, mSection, mKey))
      return settings.getSection(mFile, mSection).exists(mKey);
    else
      return NativeConfig.isOverridden(mFile, mSection, mKey);
  }

  @Override
  public boolean isRuntimeEditable()
  {
    for (StringSetting setting : NOT_RUNTIME_EDITABLE)
    {
      if (setting == this)
        return false;
    }

    return NativeConfig.isSettingSaveable(mFile, mSection, mKey);
  }

  @Override
  public boolean delete(Settings settings)
  {
    if (NativeConfig.isSettingSaveable(mFile, mSection, mKey))
    {
      return NativeConfig.deleteKey(settings.getWriteLayer(), mFile, mSection, mKey);
    }
    else
    {
      return settings.getSection(mFile, mSection).delete(mKey);
    }
  }

  @Override
  public String getString(Settings settings)
  {
    if (NativeConfig.isSettingSaveable(mFile, mSection, mKey))
    {
      return NativeConfig.getString(NativeConfig.LAYER_ACTIVE, mFile, mSection, mKey,
              mDefaultValue);
    }
    else
    {
      return settings.getSection(mFile, mSection).getString(mKey, mDefaultValue);
    }
  }

  @Override
  public void setString(Settings settings, String newValue)
  {
    if (NativeConfig.isSettingSaveable(mFile, mSection, mKey))
    {
      NativeConfig.setString(settings.getWriteLayer(), mFile, mSection, mKey, newValue);
    }
    else
    {
      settings.getSection(mFile, mSection).setString(mKey, newValue);
    }
  }

  public String getStringGlobal()
  {
    return NativeConfig.getString(NativeConfig.LAYER_ACTIVE, mFile, mSection, mKey, mDefaultValue);
  }

  public void setStringGlobal(int layer, String newValue)
  {
    NativeConfig.setString(layer, mFile, mSection, mKey, newValue);
  }
}
