package org.dolphinemu.dolphinemu.activities;

import android.os.Environment;
import android.support.annotation.Nullable;

import com.nononsenseapps.filepicker.AbstractFilePickerFragment;
import com.nononsenseapps.filepicker.FilePickerActivity;

import org.dolphinemu.dolphinemu.fragments.CustomFilePickerFragment;

import java.io.File;

public class CustomFilePickerActivity extends FilePickerActivity

{
  @Override
  protected AbstractFilePickerFragment<File> getFragment(
          @Nullable final String startPath, final int mode, final boolean allowMultiple,
          final boolean allowCreateDir, final boolean allowExistingFile,
          final boolean singleClick)
  {
    AbstractFilePickerFragment<File> fragment = new CustomFilePickerFragment();
    // startPath is allowed to be null. In that case, default folder should be SD-card and not "/"
    fragment.setArgs(
            startPath != null ? startPath : Environment.getExternalStorageDirectory().getPath(),
            mode, allowMultiple, allowCreateDir, allowExistingFile, singleClick);
    return fragment;
  }
}
