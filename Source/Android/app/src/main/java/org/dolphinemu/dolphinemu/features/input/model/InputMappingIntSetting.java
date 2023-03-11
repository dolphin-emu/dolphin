// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model;

import org.dolphinemu.dolphinemu.features.input.model.controlleremu.NumericSetting;
import org.dolphinemu.dolphinemu.features.settings.model.AbstractIntSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public class InputMappingIntSetting implements AbstractIntSetting
{
  private final NumericSetting mNumericSetting;

  public InputMappingIntSetting(NumericSetting numericSetting)
  {
    mNumericSetting = numericSetting;
  }

  @Override
  public int getInt(Settings settings)
  {
    return mNumericSetting.getIntValue();
  }

  @Override
  public void setInt(Settings settings, int newValue)
  {
    mNumericSetting.setIntValue(newValue);
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
    mNumericSetting.setIntValue(mNumericSetting.getIntDefaultValue());
    return true;
  }
}
