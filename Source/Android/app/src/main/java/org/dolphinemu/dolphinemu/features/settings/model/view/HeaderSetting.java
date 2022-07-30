// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view;

import android.content.Context;

import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting;

public class HeaderSetting extends SettingsItem
{
  public HeaderSetting(Context context, int titleId, int descriptionId)
  {
    super(context, titleId, descriptionId);
  }

  @Override
  public int getType()
  {
    return SettingsItem.TYPE_HEADER;
  }

  @Override
  public AbstractSetting getSetting()
  {
    return null;
  }
}
