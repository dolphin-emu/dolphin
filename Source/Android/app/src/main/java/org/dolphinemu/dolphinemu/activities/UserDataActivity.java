// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.activities;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;

import com.google.android.material.color.MaterialColors;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.databinding.ActivityUserDataBinding;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.utils.InsetsHelper;
import org.dolphinemu.dolphinemu.utils.Log;
import org.dolphinemu.dolphinemu.utils.ThemeHelper;
import org.dolphinemu.dolphinemu.utils.ThreadUtil;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;

public class UserDataActivity extends AppCompatActivity
{
  private static final int REQUEST_CODE_IMPORT = 0;
  private static final int REQUEST_CODE_EXPORT = 1;

  private static final int BUFFER_SIZE = 64 * 1024;

  private boolean sMustRestartApp = false;

  private ActivityUserDataBinding mBinding;

  public static void launch(Context context)
  {
    Intent launcher = new Intent(context, UserDataActivity.class);
    context.startActivity(launcher);
  }

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    ThemeHelper.setTheme(this);

    super.onCreate(savedInstanceState);

    mBinding = ActivityUserDataBinding.inflate(getLayoutInflater());
    setContentView(mBinding.getRoot());

    WindowCompat.setDecorFitsSystemWindows(getWindow(), false);

    boolean android_10 = Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q;
    boolean android_11 = Build.VERSION.SDK_INT >= Build.VERSION_CODES.R;
    boolean legacy = DirectoryInitialization.isUsingLegacyUserDirectory();

    int user_data_new_location = android_10 ?
            R.string.user_data_new_location_android_10 : R.string.user_data_new_location;
    mBinding.textType.setText(legacy ? R.string.user_data_old_location : user_data_new_location);

    mBinding.textPath.setText(DirectoryInitialization.getUserDirectory());

    mBinding.textAndroid11.setVisibility(android_11 && !legacy ? View.VISIBLE : View.GONE);

    mBinding.buttonOpenSystemFileManager.setVisibility(android_11 ? View.VISIBLE : View.GONE);
    mBinding.buttonOpenSystemFileManager.setOnClickListener(view -> openFileManager());

    mBinding.buttonImportUserData.setOnClickListener(view -> importUserData());

    mBinding.buttonExportUserData.setOnClickListener(view -> exportUserData());

    setSupportActionBar(mBinding.toolbarUserData);
    getSupportActionBar().setDisplayHomeAsUpEnabled(true);

    setInsets();
    ThemeHelper.enableScrollTint(this, mBinding.toolbarUserData, mBinding.appbarUserData);
  }

  @Override
  public boolean onSupportNavigateUp()
  {
    onBackPressed();
    return true;
  }

  @Override
  public void onActivityResult(int requestCode, int resultCode, Intent data)
  {
    super.onActivityResult(requestCode, resultCode, data);

    if (requestCode == REQUEST_CODE_IMPORT && resultCode == Activity.RESULT_OK)
    {
      Uri uri = data.getData();

      new MaterialAlertDialogBuilder(this)
              .setMessage(R.string.user_data_import_warning)
              .setNegativeButton(R.string.no, (dialog, i) -> dialog.dismiss())
              .setPositiveButton(R.string.yes, (dialog, i) ->
              {
                dialog.dismiss();

                ThreadUtil.runOnThreadAndShowResult(this, R.string.import_in_progress,
                        R.string.do_not_close_app,
                        () -> getResources().getString(importUserData(uri)),
                        (dialogInterface) ->
                        {
                          if (sMustRestartApp)
                          {
                            System.exit(0);
                          }
                        });
              })
              .show();
    }
    else if (requestCode == REQUEST_CODE_EXPORT && resultCode == Activity.RESULT_OK)
    {
      Uri uri = data.getData();

      ThreadUtil.runOnThreadAndShowResult(this, R.string.export_in_progress, 0,
              () -> getResources().getString(exportUserData(uri)));
    }
  }

  private void openFileManager()
  {
    try
    {
      // First, try the package name used on "normal" phones
      startActivity(getFileManagerIntent("com.google.android.documentsui"));
    }
    catch (ActivityNotFoundException e)
    {
      try
      {
        // Next, try the AOSP package name
        startActivity(getFileManagerIntent("com.android.documentsui"));
      }
      catch (ActivityNotFoundException e2)
      {
        // Activity not found. Perhaps it was removed by the OEM, or by some new Android version
        // that didn't exist at the time of writing. Not much we can do other than tell the user
        new MaterialAlertDialogBuilder(this)
                .setMessage(R.string.user_data_open_system_file_manager_failed)
                .setPositiveButton(R.string.ok, null)
                .show();
      }
    }
  }

  private Intent getFileManagerIntent(String packageName)
  {
    // Fragile, but some phones don't expose the system file manager in any better way
    Intent intent = new Intent(Intent.ACTION_MAIN);
    intent.setClassName(packageName, "com.android.documentsui.files.FilesActivity");
    intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
    return intent;
  }

  private void importUserData()
  {
    Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
    intent.setType("application/zip");
    startActivityForResult(intent, REQUEST_CODE_IMPORT);
  }

  private int importUserData(Uri source)
  {
    try
    {
      if (!isDolphinUserDataBackup(source))
      {
        return R.string.user_data_import_invalid_file;
      }

      try (InputStream is = getContentResolver().openInputStream(source))
      {
        try (ZipInputStream zis = new ZipInputStream(is))
        {
          File userDirectory = new File(DirectoryInitialization.getUserDirectory());
          String userDirectoryCanonicalized = userDirectory.getCanonicalPath() + '/';

          sMustRestartApp = true;
          deleteChildrenRecursively(userDirectory);

          DirectoryInitialization.getGameListCache(this).delete();

          ZipEntry ze;
          byte[] buffer = new byte[BUFFER_SIZE];
          while ((ze = zis.getNextEntry()) != null)
          {
            File destFile = new File(userDirectory, ze.getName());
            File destDirectory = ze.isDirectory() ? destFile : destFile.getParentFile();

            if (!destFile.getCanonicalPath().startsWith(userDirectoryCanonicalized))
            {
              Log.error("Zip file attempted path traversal! " + ze.getName());
              return R.string.user_data_import_failure;
            }

            if (!destDirectory.isDirectory() && !destDirectory.mkdirs())
            {
              throw new IOException("Failed to create directory " + destDirectory);
            }

            if (!ze.isDirectory())
            {
              try (FileOutputStream fos = new FileOutputStream(destFile))
              {
                int count;
                while ((count = zis.read(buffer)) != -1)
                {
                  fos.write(buffer, 0, count);
                }
              }

              long time = ze.getTime();
              if (time > 0)
              {
                destFile.setLastModified(time);
              }
            }
          }
        }
      }
    }
    catch (IOException | NullPointerException e)
    {
      e.printStackTrace();
      return R.string.user_data_import_failure;
    }

    return R.string.user_data_import_success;
  }

  private boolean isDolphinUserDataBackup(Uri uri) throws IOException
  {
    try (InputStream is = getContentResolver().openInputStream(uri))
    {
      try (ZipInputStream zis = new ZipInputStream(is))
      {
        ZipEntry ze;
        while ((ze = zis.getNextEntry()) != null)
        {
          String name = ze.getName();
          if (name.equals("Config/Dolphin.ini"))
          {
            return true;
          }
        }
      }
    }

    return false;
  }

  private void deleteChildrenRecursively(File directory) throws IOException
  {
    File[] children = directory.listFiles();
    if (children == null)
    {
      throw new IOException("Could not find directory " + directory);
    }
    for (File child : children)
    {
      deleteRecursively(child);
    }
  }

  private void deleteRecursively(File file) throws IOException
  {
    if (file.isDirectory())
    {
      deleteChildrenRecursively(file);
    }

    if (!file.delete())
    {
      throw new IOException("Failed to delete " + file);
    }
  }

  private void exportUserData()
  {
    Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
    intent.setType("application/zip");
    intent.putExtra(Intent.EXTRA_TITLE, "dolphin-emu.zip");
    startActivityForResult(intent, REQUEST_CODE_EXPORT);
  }

  private int exportUserData(Uri destination)
  {
    try (OutputStream os = getContentResolver().openOutputStream(destination))
    {
      try (ZipOutputStream zos = new ZipOutputStream(os))
      {
        exportUserData(zos, new File(DirectoryInitialization.getUserDirectory()), null);
      }
    }
    catch (IOException e)
    {
      e.printStackTrace();
      return R.string.user_data_export_failure;
    }

    return R.string.user_data_export_success;
  }

  private void exportUserData(ZipOutputStream zos, File input, @Nullable File pathRelativeToRoot)
          throws IOException
  {
    if (input.isDirectory())
    {
      File[] children = input.listFiles();
      if (children == null)
      {
        throw new IOException("Could not find directory " + input);
      }
      for (File child : children)
      {
        exportUserData(zos, child, new File(pathRelativeToRoot, child.getName()));
      }
      if (children.length == 0 && pathRelativeToRoot != null)
      {
        zos.putNextEntry(new ZipEntry(pathRelativeToRoot.getPath() + '/'));
      }
    }
    else
    {
      try (FileInputStream fis = new FileInputStream(input))
      {
        byte[] buffer = new byte[BUFFER_SIZE];
        ZipEntry entry = new ZipEntry(pathRelativeToRoot.getPath());
        entry.setTime(input.lastModified());
        zos.putNextEntry(entry);
        int count;
        while ((count = fis.read(buffer, 0, buffer.length)) != -1)
        {
          zos.write(buffer, 0, count);
        }
      }
    }
  }

  private void setInsets()
  {
    ViewCompat.setOnApplyWindowInsetsListener(mBinding.appbarUserData, (v, windowInsets) ->
    {
      Insets insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars());

      InsetsHelper.insetAppBar(insets, mBinding.appbarUserData);

      mBinding.scrollViewUserData.setPadding(insets.left, 0, insets.right, insets.bottom);

      InsetsHelper.applyNavbarWorkaround(insets.bottom, mBinding.workaroundView);
      ThemeHelper.setNavigationBarColor(this,
              MaterialColors.getColor(mBinding.appbarUserData, R.attr.colorSurface));

      return windowInsets;
    });
  }
}
