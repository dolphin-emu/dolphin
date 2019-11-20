package org.dolphinemu.dolphinemu.fragments;

import android.net.Uri;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.core.content.FileProvider;

import android.view.View;
import android.widget.TextView;

import org.dolphinemu.dolphinemu.R;

import com.nononsenseapps.filepicker.FilePickerFragment;

import java.io.File;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

public class CustomFilePickerFragment extends FilePickerFragment
{
  private static final Set<String> extensions = new HashSet<>(Arrays.asList(
          "gcm", "tgc", "iso", "ciso", "gcz", "wbfs", "wad", "dol", "elf", "dff"));

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
                    extensions.contains(fileExtension(file.getName()).toLowerCase()));
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
