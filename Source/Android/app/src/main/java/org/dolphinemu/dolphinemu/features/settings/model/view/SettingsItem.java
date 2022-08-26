// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view;

import android.content.Context;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter;

/**
 * ViewModel abstraction for an Item in the RecyclerView powering SettingsFragments.
 * Most of them correspond to a single line in an INI file, but there are a few with multiple
 * analogues and a few with none (Headers, for example, do not correspond to anything on disk.)
 */
public abstract class SettingsItem
{
  public static final int TYPE_HEADER = 0;
  public static final int TYPE_CHECKBOX = 1;
  public static final int TYPE_SINGLE_CHOICE = 2;
  public static final int TYPE_SLIDER = 3;
  public static final int TYPE_SUBMENU = 4;
  public static final int TYPE_INPUT_BINDING = 5;
  public static final int TYPE_STRING_SINGLE_CHOICE = 6;
  public static final int TYPE_RUMBLE_BINDING = 7;
  public static final int TYPE_SINGLE_CHOICE_DYNAMIC_DESCRIPTIONS = 8;
  public static final int TYPE_FILE_PICKER = 9;
  public static final int TYPE_RUN_RUNNABLE = 10;
  public static final int TYPE_STRING = 11;
  public static final int TYPE_HYPERLINK_HEADER = 12;

  private final CharSequence mName;
  private final CharSequence mDescription;

  /**
   * Base constructor.
   *
   * @param name        A text string to be displayed as this setting's name.
   * @param description A text string to be displayed as this setting's description.
   */
  public SettingsItem(CharSequence name, CharSequence description)
  {
    mName = name;
    mDescription = description;
  }

  /**
   * @param nameId        Resource ID for a text string to be displayed as this setting's name.
   * @param descriptionId Resource ID for a text string to be displayed as this setting's description.
   */
  public SettingsItem(Context context, int nameId, int descriptionId)
  {
    mName = nameId == 0 ? "" : context.getText(nameId);
    mDescription = descriptionId == 0 ? "" : context.getText(descriptionId);
  }

  public CharSequence getName()
  {
    return mName;
  }

  public CharSequence getDescription()
  {
    return mDescription;
  }

  /**
   * Used by {@link SettingsAdapter}'s onCreateViewHolder()
   * method to determine which type of ViewHolder should be created.
   *
   * @return An integer (ideally, one of the constants defined in this file)
   */
  public abstract int getType();

  protected abstract AbstractSetting getSetting();

  public boolean isOverridden(Settings settings)
  {
    AbstractSetting setting = getSetting();
    return setting != null && setting.isOverridden(settings);
  }

  public boolean isEditable()
  {
    if (!NativeLibrary.IsRunning())
      return true;

    AbstractSetting setting = getSetting();
    return setting != null && setting.isRuntimeEditable();
  }

  public boolean hasSetting()
  {
    return getSetting() != null;
  }

  public void clear(Settings settings)
  {
    getSetting().delete(settings);
  }
}
