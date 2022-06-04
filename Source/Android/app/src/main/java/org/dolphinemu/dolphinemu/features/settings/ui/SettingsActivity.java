// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui;

import android.app.ProgressDialog;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Settings;
import android.view.Menu;
import android.view.MenuInflater;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.FragmentTransaction;
import androidx.lifecycle.ViewModelProvider;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.ui.main.MainPresenter;
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper;

import java.util.Set;

public final class SettingsActivity extends AppCompatActivity implements SettingsActivityView
{
  private static final String ARG_MENU_TAG = "menu_tag";
  private static final String ARG_GAME_ID = "game_id";
  private static final String ARG_REVISION = "revision";
  private static final String ARG_IS_WII = "is_wii";
  private static final String FRAGMENT_TAG = "settings";
  private SettingsActivityPresenter mPresenter;

  private ProgressDialog dialog;

  public static void launch(Context context, MenuTag menuTag, String gameId, int revision,
          boolean isWii)
  {
    Intent settings = new Intent(context, SettingsActivity.class);
    settings.putExtra(ARG_MENU_TAG, menuTag);
    settings.putExtra(ARG_GAME_ID, gameId);
    settings.putExtra(ARG_REVISION, revision);
    settings.putExtra(ARG_IS_WII, isWii);
    context.startActivity(settings);
  }

  public static void launch(Context context, MenuTag menuTag)
  {
    Intent settings = new Intent(context, SettingsActivity.class);
    settings.putExtra(ARG_MENU_TAG, menuTag);
    settings.putExtra(ARG_IS_WII, !NativeLibrary.IsRunning() || NativeLibrary.IsEmulatingWii());
    context.startActivity(settings);
  }

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);

    // If we came here from the game list, we don't want to rescan when returning to the game list.
    // But if we came here after UserDataActivity restarted the app, we do want to rescan.
    if (savedInstanceState == null)
    {
      MainPresenter.skipRescanningLibrary();
    }

    setContentView(R.layout.activity_settings);

    Intent launcher = getIntent();
    String gameID = launcher.getStringExtra(ARG_GAME_ID);
    if (gameID == null)
      gameID = "";
    int revision = launcher.getIntExtra(ARG_REVISION, 0);
    boolean isWii = launcher.getBooleanExtra(ARG_IS_WII, true);
    MenuTag menuTag = (MenuTag) launcher.getSerializableExtra(ARG_MENU_TAG);

    mPresenter = new SettingsActivityPresenter(this, getSettings());
    mPresenter.onCreate(savedInstanceState, menuTag, gameID, revision, isWii, this);

    // show up button
    getSupportActionBar().setDisplayHomeAsUpEnabled(true);
  }

  @Override
  public boolean onCreateOptionsMenu(Menu menu)
  {
    MenuInflater inflater = getMenuInflater();
    inflater.inflate(R.menu.menu_settings, menu);

    return true;
  }

  @Override
  protected void onSaveInstanceState(@NonNull Bundle outState)
  {
    // Critical: If super method is not called, rotations will be busted.
    super.onSaveInstanceState(outState);
    mPresenter.saveState(outState);
  }

  @Override
  protected void onStart()
  {
    super.onStart();
    mPresenter.onStart();
  }

  /**
   * If this is called, the user has left the settings screen (potentially through the
   * home button) and will expect their changes to be persisted.
   */
  @Override
  protected void onStop()
  {
    super.onStop();
    mPresenter.onStop(isFinishing());
  }

  @Override
  protected void onDestroy()
  {
    super.onDestroy();
    mPresenter.onDestroy();
  }

  @Override
  public void showSettingsFragment(MenuTag menuTag, Bundle extras, boolean addToStack,
          String gameID)
  {
    if (!addToStack && getFragment() != null)
      return;

    FragmentTransaction transaction = getSupportFragmentManager().beginTransaction();

    if (addToStack)
    {
      if (areSystemAnimationsEnabled())
      {
        transaction.setCustomAnimations(
                R.animator.settings_enter,
                R.animator.settings_exit,
                R.animator.settings_pop_enter,
                R.animator.setttings_pop_exit);
      }

      transaction.addToBackStack(null);
    }
    transaction.replace(R.id.frame_content, SettingsFragment.newInstance(menuTag, gameID, extras),
            FRAGMENT_TAG);

    transaction.commit();
  }

  private boolean areSystemAnimationsEnabled()
  {
    float duration = Settings.Global.getFloat(
            getContentResolver(),
            Settings.Global.ANIMATOR_DURATION_SCALE, 1);
    float transition = Settings.Global.getFloat(
            getContentResolver(),
            Settings.Global.TRANSITION_ANIMATION_SCALE, 1);
    return duration != 0 && transition != 0;
  }

  @Override
  protected void onActivityResult(int requestCode, int resultCode, Intent result)
  {
    super.onActivityResult(requestCode, resultCode, result);

    // If the user picked a file, as opposed to just backing out.
    if (resultCode == RESULT_OK)
    {
      if (requestCode != MainPresenter.REQUEST_DIRECTORY)
      {
        Uri uri = canonicalizeIfPossible(result.getData());

        Set<String> validExtensions = requestCode == MainPresenter.REQUEST_GAME_FILE ?
                FileBrowserHelper.GAME_EXTENSIONS : FileBrowserHelper.RAW_EXTENSION;

        int flags = Intent.FLAG_GRANT_READ_URI_PERMISSION;
        if (requestCode != MainPresenter.REQUEST_GAME_FILE)
          flags |= Intent.FLAG_GRANT_WRITE_URI_PERMISSION;
        int takeFlags = flags & result.getFlags();

        FileBrowserHelper.runAfterExtensionCheck(this, uri, validExtensions, () ->
        {
          getContentResolver().takePersistableUriPermission(uri, takeFlags);
          getFragment().getAdapter().onFilePickerConfirmation(uri.toString());
        });
      }
      else
      {
        String path = FileBrowserHelper.getSelectedPath(result);
        getFragment().getAdapter().onFilePickerConfirmation(path);
      }
    }
  }

  @NonNull
  private Uri canonicalizeIfPossible(@NonNull Uri uri)
  {
    Uri canonicalizedUri = getContentResolver().canonicalize(uri);
    return canonicalizedUri != null ? canonicalizedUri : uri;
  }

  @Override
  public void showLoading()
  {
    if (dialog == null)
    {
      dialog = new ProgressDialog(this);
      dialog.setMessage(getString(R.string.load_settings));
      dialog.setIndeterminate(true);
    }

    dialog.show();
  }

  @Override
  public void hideLoading()
  {
    dialog.dismiss();
  }

  @Override
  public void showGameIniJunkDeletionQuestion()
  {
    new AlertDialog.Builder(this)
            .setTitle(getString(R.string.game_ini_junk_title))
            .setMessage(getString(R.string.game_ini_junk_question))
            .setPositiveButton(R.string.yes, (dialogInterface, i) -> mPresenter.clearSettings())
            .setNegativeButton(R.string.no, null)
            .show();
  }

  @Override
  public org.dolphinemu.dolphinemu.features.settings.model.Settings getSettings()
  {
    return new ViewModelProvider(this).get(SettingsViewModel.class).getSettings();
  }

  @Override
  public void onSettingsFileLoaded(
          org.dolphinemu.dolphinemu.features.settings.model.Settings settings)
  {
    SettingsFragmentView fragment = getFragment();

    if (fragment != null)
    {
      fragment.onSettingsFileLoaded(settings);
    }
  }

  @Override
  public void onSettingsFileNotFound()
  {
    SettingsFragmentView fragment = getFragment();

    if (fragment != null)
    {
      fragment.loadDefaultSettings();
    }
  }

  @Override
  public void showToastMessage(String message)
  {
    Toast.makeText(this, message, Toast.LENGTH_SHORT).show();
  }

  @Override
  public void onSettingChanged()
  {
    mPresenter.onSettingChanged();
  }

  @Override
  public void onGcPadSettingChanged(MenuTag key, int value)
  {
    mPresenter.onGcPadSettingChanged(key, value);
  }

  @Override
  public void onWiimoteSettingChanged(MenuTag section, int value)
  {
    mPresenter.onWiimoteSettingChanged(section, value);
  }

  @Override
  public void onExtensionSettingChanged(MenuTag menuTag, int value)
  {
    mPresenter.onExtensionSettingChanged(menuTag, value);
  }

  @Override
  public boolean onSupportNavigateUp()
  {
    onBackPressed();
    return true;
  }

  private SettingsFragment getFragment()
  {
    return (SettingsFragment) getSupportFragmentManager().findFragmentByTag(FRAGMENT_TAG);
  }
}
