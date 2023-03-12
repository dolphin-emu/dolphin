// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model;

import org.dolphinemu.dolphinemu.features.input.model.controlleremu.ControlGroup;
import org.dolphinemu.dolphinemu.features.settings.model.AbstractBooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public class ControlGroupEnabledSetting implements AbstractBooleanSetting
{
  private final ControlGroup mControlGroup;

  public ControlGroupEnabledSetting(ControlGroup controlGroup)
  {
    mControlGroup = controlGroup;
  }

  @Override
  public boolean getBoolean(Settings settings)
  {
    return mControlGroup.getEnabled();
  }

  @Override
  public void setBoolean(Settings settings, boolean newValue)
  {
    mControlGroup.setEnabled(newValue);
  }

  @Override
  public boolean isOverridden(Settings settings)
  {
    return false;
  }

  @Override
  public boolean isRuntimeEditable()
  {
    return true;
  }

  @Override
  public boolean delete(Settings settings)
  {
    boolean newValue = mControlGroup.getDefaultEnabledValue() != ControlGroup.DEFAULT_ENABLED_NO;
    mControlGroup.setEnabled(newValue);

    return true;
  }
}
