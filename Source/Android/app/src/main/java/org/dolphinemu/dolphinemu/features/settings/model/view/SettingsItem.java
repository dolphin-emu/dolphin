package org.dolphinemu.dolphinemu.features.settings.model.view;

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
  public static final int TYPE_CONFIRM_RUNNABLE = 10;

  private String mFile;
  private String mSection;
  private String mKey;

  private int mNameId;
  private int mDescriptionId;

  /**
   * Base constructor.
   *
   * @param file          File to which the Setting belongs.
   * @param section       Section to which the Setting belongs.
   * @param key           Identifier for the Setting represented by this Item.
   * @param nameId        Resource ID for a text string to be displayed as this setting's name.
   * @param descriptionId Resource ID for a text string to be displayed as this setting's description.
   */

  public SettingsItem(String file, String section, String key, int nameId, int descriptionId)
  {
    mFile = file;
    mSection = section;
    mKey = key;
    mNameId = nameId;
    mDescriptionId = descriptionId;
  }

  /**
   * @return The file in which the backing setting belongs.
   */
  public String getFile()
  {
    return mFile;
  }

  /**
   * @return The header under which the backing setting belongs.
   */
  public String getSection()
  {
    return mSection;
  }

  /**
   * @return The identifier for the backing setting.
   */
  public String getKey()
  {
    return mKey;
  }

  /**
   * @return A resource ID for a text string representing this setting's name.
   */
  public int getNameId()
  {
    return mNameId;
  }

  public int getDescriptionId()
  {
    return mDescriptionId;
  }

  /**
   * Used by {@link SettingsAdapter}'s onCreateViewHolder()
   * method to determine which type of ViewHolder should be created.
   *
   * @return An integer (ideally, one of the constants defined in this file)
   */
  public abstract int getType();
}
