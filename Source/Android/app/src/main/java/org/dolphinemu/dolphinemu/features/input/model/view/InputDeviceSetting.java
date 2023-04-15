// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model.view;

import android.content.Context;

import org.dolphinemu.dolphinemu.features.input.model.ControllerInterface;
import org.dolphinemu.dolphinemu.features.input.model.controlleremu.EmulatedController;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.model.view.StringSingleChoiceSetting;

public class InputDeviceSetting extends StringSingleChoiceSetting
{
  private final EmulatedController mController;

  public InputDeviceSetting(Context context, int titleId, int descriptionId,
          EmulatedController controller)
  {
    super(context, null, titleId, descriptionId, null, null, null);

    mController = controller;

    refreshChoicesAndValues();
  }

  @Override
  public String getSelectedChoice()
  {
    return mController.getDefaultDevice();
  }

  @Override
  public String getSelectedValue()
  {
    return mController.getDefaultDevice();
  }

  @Override
  public void setSelectedValue(Settings settings, String newValue)
  {
    mController.setDefaultDevice(newValue);
  }

  @Override
  public void refreshChoicesAndValues()
  {
    String[] devices = ControllerInterface.getAllDeviceStrings();

    setChoices(devices);
    setValues(devices);
  }

  @Override
  public boolean isEditable()
  {
    return true;
  }

  @Override
  public boolean canClear()
  {
    return true;
  }

  @Override
  public void clear(Settings settings)
  {
    setSelectedValue(settings, "");
  }
}
