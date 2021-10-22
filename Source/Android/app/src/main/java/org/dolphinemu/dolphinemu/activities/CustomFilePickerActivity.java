// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.activities;

import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;

import androidx.annotation.Nullable;

import com.nononsenseapps.filepicker.AbstractFilePickerFragment;
import com.nononsenseapps.filepicker.FilePickerActivity;

import org.dolphinemu.dolphinemu.fragments.CustomFilePickerFragment;

import java.io.File;
import java.util.HashSet;

public class CustomFilePickerActivity extends FilePickerActivity
{
  public static final String EXTRA_EXTENSIONS = "dolphinemu.org.filepicker.extensions";

  private HashSet<String> mExtensions;

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);

    Intent intent = getIntent();
    if (intent != null)
    {
      mExtensions = (HashSet<String>) intent.getSerializableExtra(EXTRA_EXTENSIONS);
    }
  }

  @Override
  protected AbstractFilePickerFragment<File> getFragment(
          @Nullable final String startPath, final int mode, final boolean allowMultiple,
          final boolean allowCreateDir, final boolean allowExistingFile,
          final boolean singleClick)
  {
    CustomFilePickerFragment fragment = new CustomFilePickerFragment();
    // startPath is allowed to be null. In that case, default folder should be SD-card and not "/"
    fragment.setArgs(
            startPath != null ? startPath : Environment.getExternalStorageDirectory().getPath(),
            mode, allowMultiple, allowCreateDir, allowExistingFile, singleClick);
    fragment.setExtensions(mExtensions);
    return fragment;
  }
}
