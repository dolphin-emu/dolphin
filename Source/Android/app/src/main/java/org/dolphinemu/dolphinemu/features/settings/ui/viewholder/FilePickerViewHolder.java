// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder;

import android.text.TextUtils;
import android.view.View;

import androidx.annotation.Nullable;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.databinding.ListItemSettingBinding;
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

  private final ListItemSettingBinding mBinding;

  public FilePickerViewHolder(ListItemSettingBinding binding, SettingsAdapter adapter)
  {
    super(binding.getRoot(), adapter);
    mBinding = binding;
  }

  @Override
  public void bind(SettingsItem item)
  {
    mFilePicker = (FilePicker) item;
    mItem = item;

    String path = mFilePicker.getSelectedValue(getAdapter().getSettings());

    if (FileBrowserHelper.isPathEmptyOrValid(path))
    {
      itemView.setBackground(mBinding.getRoot().getBackground());
    }
    else
    {
      itemView.setBackgroundResource(R.drawable.invalid_setting_background);
    }

    mBinding.textSettingName.setText(item.getName());

    if (!TextUtils.isEmpty(item.getDescription()))
    {
      mBinding.textSettingDescription.setText(item.getDescription());
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

      mBinding.textSettingDescription.setText(path);
    }

    setStyle(mBinding.textSettingName, mItem);
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

    setStyle(mBinding.textSettingName, mItem);
  }

  @Nullable @Override
  protected SettingsItem getItem()
  {
    return mItem;
  }
}
