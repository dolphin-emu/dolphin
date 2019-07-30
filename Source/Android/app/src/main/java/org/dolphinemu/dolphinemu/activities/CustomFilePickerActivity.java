package org.dolphinemu.dolphinemu.activities;

import android.net.Uri;
import android.os.Environment;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.content.FileProvider;

import com.nononsenseapps.filepicker.AbstractFilePickerFragment;
import com.nononsenseapps.filepicker.FilePickerActivity;
import com.nononsenseapps.filepicker.FilePickerFragment;

import java.io.File;

public class CustomFilePickerActivity extends FilePickerActivity

{
  public static class CustomFilePickerFragment extends FilePickerFragment
  {
    @NonNull
    @Override
    public Uri toUri(@NonNull final File file)
    {
      return FileProvider
        .getUriForFile(getContext(),
          getContext().getApplicationContext().getPackageName() + ".filesprovider",
          file);
    }
  }

  @Override
  protected AbstractFilePickerFragment<File> getFragment(
    @Nullable final String startPath, final int mode, final boolean allowMultiple,
    final boolean allowExistingFile, final boolean singleClick)
  {
    AbstractFilePickerFragment<File> fragment = new CustomFilePickerFragment();
    // startPath is allowed to be null. In that case, default folder should be SD-card and not "/"
    fragment.setArgs(
      startPath != null ? startPath : Environment.getExternalStorageDirectory().getPath(),
      mode, allowMultiple, allowExistingFile, singleClick);
    return fragment;
  }
}
