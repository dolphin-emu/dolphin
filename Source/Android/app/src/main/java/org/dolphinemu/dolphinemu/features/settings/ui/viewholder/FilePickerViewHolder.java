package org.dolphinemu.dolphinemu.features.settings.ui.viewholder;

import android.view.View;
import android.widget.TextView;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.view.FilePicker;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter;
import org.dolphinemu.dolphinemu.ui.main.MainPresenter;
import org.dolphinemu.dolphinemu.utils.IniFile;

public final class FilePickerViewHolder extends SettingViewHolder
{
  private FilePicker mFilePicker;
  private SettingsItem mItem;

  private TextView mTextSettingName;
  private TextView mTextSettingDescription;

  public FilePickerViewHolder(View itemView, SettingsAdapter adapter)
  {
    super(itemView, adapter);
  }

  @Override
  protected void findViews(View root)
  {
    mTextSettingName = root.findViewById(R.id.text_setting_name);
    mTextSettingDescription = root.findViewById(R.id.text_setting_description);
  }

  @Override
  public void bind(SettingsItem item)
  {
    mFilePicker = (FilePicker) item;
    mItem = item;

    mTextSettingName.setText(item.getNameId());

    if (item.getDescriptionId() > 0)
    {
      mTextSettingDescription.setText(item.getDescriptionId());
    }
    else
    {
      // TODO: Reopening INI files all the time is slow
      IniFile ini = new IniFile(mFilePicker.getFile());
      mTextSettingDescription.setText(ini.getString(item.getSection(), item.getKey(),
              mFilePicker.getSelectedValue()));
    }
  }

  @Override
  public void onClick(View clicked)
  {
    if (mFilePicker.getRequestType() == MainPresenter.REQUEST_DIRECTORY)
    {
      getAdapter().onFilePickerDirectoryClick(mItem);
    }
    else
    {
      getAdapter().onFilePickerFileClick(mItem);
    }
  }
}
