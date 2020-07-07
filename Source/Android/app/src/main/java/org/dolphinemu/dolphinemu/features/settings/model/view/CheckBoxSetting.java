package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.utils.SettingsFile;

public final class CheckBoxSetting extends SettingsItem
{
  private boolean mDefaultValue;

  public CheckBoxSetting(String file, String section, String key, int titleId, int descriptionId,
          boolean defaultValue)
  {
    super(file, section, key, titleId, descriptionId);
    mDefaultValue = defaultValue;
  }

  public boolean isChecked(Settings settings)
  {
    return invertIfNeeded(settings.getSection(getFile(), getSection())
            .getBoolean(getKey(), invertIfNeeded(mDefaultValue)));
  }

  public void setChecked(Settings settings, boolean checked)
  {
    settings.getSection(getFile(), getSection()).setBoolean(getKey(), invertIfNeeded(checked));
  }

  private boolean invertIfNeeded(boolean x)
  {
    return isInverted() ? !x : x;
  }

  private boolean isInverted()
  {
    return getKey().equals(SettingsFile.KEY_SKIP_EFB) ||
            getKey().equals(SettingsFile.KEY_IGNORE_FORMAT);
  }

  @Override
  public int getType()
  {
    return TYPE_CHECKBOX;
  }
}
