// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder;

import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.view.FilePicker;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter;
import org.dolphinemu.dolphinemu.ui.main.MainPresenter;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper;

public final class FilePickerViewHolder extends SettingViewHolder
{
  private FilePicker mFilePicker;
  private SettingsItem mItem;

  private TextView mTextSettingName;
  private TextView mTextSettingDescription;

  private Drawable mDefaultBackground;

  public FilePickerViewHolder(View itemView, SettingsAdapter adapter)
  {
    super(itemView, adapter);
  }

  @Override
  protected void findViews(View root)
  {
    mTextSettingName = root.findViewById(R.id.text_setting_name);
    mTextSettingDescription = root.findViewById(R.id.text_setting_description);

    mDefaultBackground = root.getBackground();
  }

  @Override
  public void bind(SettingsItem item)
  {
    mFilePicker = (FilePicker) item;
    mItem = item;

    String path = mFilePicker.getSelectedValue(getAdapter().getSettings());

    if (FileBrowserHelper.isPathEmptyOrValid(path))
    {
      itemView.setBackground(mDefaultBackground);
    }
    else
    {
      itemView.setBackgroundResource(R.drawable.invalid_setting_background);
    }

    mTextSettingName.setText(item.getName());

    if (!TextUtils.isEmpty(item.getDescription()))
    {
      mTextSettingDescription.setText(item.getDescription());
    }
    else
    {
      if (TextUtils.isEmpty(path))
      {
        String defaultPathRelative = mFilePicker.getDefaultPathRelativeToUserDirectory();
        if (defaultPathRelative != null)
        {
          path = DirectoryInitialization.getUserDirectory() + defaultPathRelative;
        }
      }

      mTextSettingDescription.setText(path);
    }

    setStyle(mTextSettingName, mItem);
  }

  @Override
  public void onClick(View clicked)
  {
    if (!mItem.isEditable())
    {
      showNotRuntimeEditableError();
      return;
    }

    int position = getBindingAdapterPosition();
    if (mFilePicker.getRequestType() == MainPresenter.REQUEST_DIRECTORY)
    {
      getAdapter().onFilePickerDirectoryClick(mItem, position);
    }
    else
    {
      getAdapter().onFilePickerFileClick(mItem, position);
    }

    setStyle(mTextSettingName, mItem);
  }

  @Nullable @Override
  protected SettingsItem getItem()
  {
    return mItem;
  }
}
