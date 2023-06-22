// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view;

import android.content.Context;

public final class HyperLinkHeaderSetting extends HeaderSetting
{
  public HyperLinkHeaderSetting(Context context, int titleId, int descriptionId)
  {
    super(context, titleId, descriptionId);
  }

  @Override
  public int getType()
  {
    return SettingsItem.TYPE_HYPERLINK_HEADER;
  }
}
