package org.dolphinemu.dolphinemu.features.input.model;

import android.view.Display;

import androidx.annotation.NonNull;

public interface SensorEventRequester
{
  /**
   * Returns the display the activity is shown on.
   *
   * This is used for getting the display orientation for rotating the axes of motion events.
   */
  @NonNull
  Display getDisplay();
}
