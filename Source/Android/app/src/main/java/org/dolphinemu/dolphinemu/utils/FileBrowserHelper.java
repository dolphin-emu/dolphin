package org.dolphinemu.dolphinemu.utils;

import android.content.Intent;
import android.net.Uri;
import android.os.Environment;

import androidx.annotation.Nullable;
import androidx.fragment.app.FragmentActivity;

import com.nononsenseapps.filepicker.FilePickerActivity;
import com.nononsenseapps.filepicker.Utils;

import org.dolphinemu.dolphinemu.activities.CustomFilePickerActivity;
import org.dolphinemu.dolphinemu.ui.main.MainPresenter;

import java.io.File;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;

public final class FileBrowserHelper
{
  public static final HashSet<String> GAME_EXTENSIONS = new HashSet<>(Arrays.asList(
          "gcm", "tgc", "iso", "ciso", "gcz", "wbfs", "wia", "rvz", "wad", "dol", "elf", "dff"));

  public static final HashSet<String> RAW_EXTENSION = new HashSet<>(Collections.singletonList(
          "raw"));

  public static final HashSet<String> WAD_EXTENSION = new HashSet<>(Collections.singletonList(
          "wad"));

  public static void openDirectoryPicker(FragmentActivity activity, HashSet<String> extensions)
  {
    Intent i = new Intent(activity, CustomFilePickerActivity.class);

    i.putExtra(FilePickerActivity.EXTRA_ALLOW_MULTIPLE, false);
    i.putExtra(FilePickerActivity.EXTRA_ALLOW_CREATE_DIR, false);
    i.putExtra(FilePickerActivity.EXTRA_MODE, FilePickerActivity.MODE_DIR);
    i.putExtra(FilePickerActivity.EXTRA_START_PATH,
            Environment.getExternalStorageDirectory().getPath());
    i.putExtra(CustomFilePickerActivity.EXTRA_EXTENSIONS, extensions);

    activity.startActivityForResult(i, MainPresenter.REQUEST_DIRECTORY);
  }

  public static void openFilePicker(FragmentActivity activity, int requestCode, boolean allowMulti,
          HashSet<String> extensions)
  {
    Intent i = new Intent(activity, CustomFilePickerActivity.class);

    i.putExtra(FilePickerActivity.EXTRA_ALLOW_MULTIPLE, allowMulti);
    i.putExtra(FilePickerActivity.EXTRA_ALLOW_CREATE_DIR, false);
    i.putExtra(FilePickerActivity.EXTRA_MODE, FilePickerActivity.MODE_FILE);
    i.putExtra(FilePickerActivity.EXTRA_START_PATH,
            Environment.getExternalStorageDirectory().getPath());
    i.putExtra(CustomFilePickerActivity.EXTRA_EXTENSIONS, extensions);

    activity.startActivityForResult(i, requestCode);
  }

  @Nullable
  public static String getSelectedPath(Intent result)
  {
    // Use the provided utility method to parse the result
    List<Uri> files = Utils.getSelectedFilesFromResult(result);
    if (!files.isEmpty())
    {
      File file = Utils.getFileForUri(files.get(0));
      return file.getAbsolutePath();
    }

    return null;
  }

  @Nullable
  public static String[] getSelectedFiles(Intent result)
  {
    // Use the provided utility method to parse the result
    List<Uri> files = Utils.getSelectedFilesFromResult(result);
    if (!files.isEmpty())
    {
      String[] paths = new String[files.size()];
      for (int i = 0; i < files.size(); i++)
        paths[i] = Utils.getFileForUri(files.get(i)).getAbsolutePath();
      return paths;
    }

    return null;
  }
}
