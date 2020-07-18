package org.dolphinemu.dolphinemu.features.settings.model.view;

public final class HeaderSetting extends SettingsItem
{
  public HeaderSetting(int titleId, int descriptionId)
  {
    super(titleId, descriptionId);
  }

  @Override
  public int getType()
  {
    return SettingsItem.TYPE_HEADER;
  }
}
