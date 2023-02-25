// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui;

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
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.fragment.app.FragmentTransaction;
import androidx.lifecycle.ViewModelProvider;

import com.google.android.material.appbar.CollapsingToolbarLayout;
import com.google.android.material.color.MaterialColors;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.databinding.ActivitySettingsBinding;
import org.dolphinemu.dolphinemu.ui.main.MainPresenter;
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper;
import org.dolphinemu.dolphinemu.utils.InsetsHelper;
import org.dolphinemu.dolphinemu.utils.ThemeHelper;

import java.util.Set;

public final class SettingsActivity extends AppCompatActivity implements SettingsActivityView
{
  private static final String ARG_MENU_TAG = "menu_tag";
  private static final String ARG_GAME_ID = "game_id";
  private static final String ARG_REVISION = "revision";
  private static final String ARG_IS_WII = "is_wii";
  private static final String FRAGMENT_TAG = "settings";
  private SettingsActivityPresenter mPresenter;

  private AlertDialog dialog;

  private CollapsingToolbarLayout mToolbarLayout;

  private ActivitySettingsBinding mBinding;

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
    ThemeHelper.setTheme(this);

    super.onCreate(savedInstanceState);

    // If we came here from the game list, we don't want to rescan when returning to the game list.
    // But if we came here after UserDataActivity restarted the app, we do want to rescan.
    if (savedInstanceState == null)
    {
      MainPresenter.skipRescanningLibrary();
    }

    mBinding = ActivitySettingsBinding.inflate(getLayoutInflater());
    setContentView(mBinding.getRoot());

    WindowCompat.setDecorFitsSystemWindows(getWindow(), false);

    Intent launcher = getIntent();
    String gameID = launcher.getStringExtra(ARG_GAME_ID);
    if (gameID == null)
      gameID = "";
    int revision = launcher.getIntExtra(ARG_REVISION, 0);
    boolean isWii = launcher.getBooleanExtra(ARG_IS_WII, true);
    MenuTag menuTag = (MenuTag) launcher.getSerializableExtra(ARG_MENU_TAG);

    mPresenter = new SettingsActivityPresenter(this, getSettings());
    mPresenter.onCreate(savedInstanceState, menuTag, gameID, revision, isWii, this);

    mToolbarLayout = mBinding.toolbarSettingsLayout;
    setSupportActionBar(mBinding.toolbarSettings);
    getSupportActionBar().setDisplayHomeAsUpEnabled(true);

    // TODO: Remove this when CollapsingToolbarLayouts are fixed by Google
    // https://github.com/material-components/material-components-android/issues/1310
    ViewCompat.setOnApplyWindowInsetsListener(mToolbarLayout, null);

    setInsets();
    ThemeHelper.enableScrollTint(this, mBinding.toolbarSettings, mBinding.appbarSettings);
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
                R.anim.anim_settings_fragment_in,
                R.anim.anim_settings_fragment_out,
                0,
                R.anim.anim_pop_settings_fragment_out);
      }

      transaction.addToBackStack(null);
    }
    transaction.replace(R.id.frame_content_settings,
            SettingsFragment.newInstance(menuTag, gameID, extras), FRAGMENT_TAG);

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
      dialog = new MaterialAlertDialogBuilder(this)
              .setTitle(getString(R.string.load_settings))
              .setView(R.layout.dialog_indeterminate_progress)
              .create();
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
    new MaterialAlertDialogBuilder(this)
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
  public void onSerialPort1SettingChanged(MenuTag menuTag, int value)
  {
    mPresenter.onSerialPort1SettingChanged(menuTag, value);
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

  public void setToolbarTitle(String title)
  {
    mToolbarLayout.setTitle(title);
  }

  private void setInsets()
  {
    ViewCompat.setOnApplyWindowInsetsListener(mBinding.appbarSettings, (v, windowInsets) ->
    {
      Insets insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars());

      InsetsHelper.insetAppBar(insets, mBinding.appbarSettings);

      mBinding.frameContentSettings.setPadding(insets.left, 0, insets.right, 0);

      InsetsHelper.applyNavbarWorkaround(insets.bottom, mBinding.workaroundView);
      ThemeHelper.setNavigationBarColor(this,
              MaterialColors.getColor(mBinding.appbarSettings, R.attr.colorSurface));

      return windowInsets;
    });
  }
}
