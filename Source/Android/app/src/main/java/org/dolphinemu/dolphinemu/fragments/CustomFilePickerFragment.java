// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.fragments;

import android.net.Uri;
import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.core.content.FileProvider;

import com.nononsenseapps.filepicker.FilePickerFragment;

import org.dolphinemu.dolphinemu.R;

import java.io.File;
import java.util.HashSet;

public class CustomFilePickerFragment extends FilePickerFragment
{
  public static final String KEY_EXTENSIONS = "KEY_EXTENSIONS";

  private HashSet<String> mExtensions;

  public void setExtensions(HashSet<String> extensions)
  {
    Bundle b = getArguments();
    if (b == null)
      b = new Bundle();

    b.putSerializable(KEY_EXTENSIONS, extensions);
    setArguments(b);
  }

  @NonNull
  @Override
  public Uri toUri(@NonNull final File file)
  {
    return FileProvider
            .getUriForFile(getContext(),
                    getContext().getApplicationContext().getPackageName() + ".filesprovider",
                    file);
  }

  @Override public void onActivityCreated(Bundle savedInstanceState)
  {
    super.onActivityCreated(savedInstanceState);

    mExtensions = (HashSet<String>) getArguments().getSerializable(KEY_EXTENSIONS);

    if (mode == MODE_DIR)
    {
      TextView ok = getActivity().findViewById(R.id.nnf_button_ok);
      ok.setText(R.string.select_dir);

      TextView cancel = getActivity().findViewById(R.id.nnf_button_cancel);
      cancel.setVisibility(View.GONE);
    }
  }

  @Override
  protected boolean isItemVisible(@NonNull final File file)
  {
    // Some users jump to the conclusion that Dolphin isn't able to detect their
    // files if the files don't show up in the file picker when mode == MODE_DIR.
    // To avoid this, show files even when the user needs to select a directory.

    return (showHiddenItems || !file.isHidden()) &&
            (file.isDirectory() ||
                    mExtensions.contains(fileExtension(file.getName()).toLowerCase()));
  }

  @Override
  public boolean isCheckable(@NonNull final File file)
  {
    // We need to make a small correction to the isCheckable logic due to
    // overriding isItemVisible to show files when mode == MODE_DIR.
    // AbstractFilePickerFragment always treats files as checkable when
    // allowExistingFile == true, but we don't want files to be checkable when mode == MODE_DIR.

    return super.isCheckable(file) && !(mode == MODE_DIR && file.isFile());
  }

  private static String fileExtension(@NonNull String filename)
  {
    int i = filename.lastIndexOf('.');
    return i < 0 ? "" : filename.substring(i + 1);
  }
}
