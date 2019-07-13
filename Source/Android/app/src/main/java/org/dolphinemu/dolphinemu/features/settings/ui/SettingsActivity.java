package org.dolphinemu.dolphinemu.features.settings.ui;

import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.provider.Settings;
import android.support.v4.app.FragmentTransaction;
import android.support.v4.content.LocalBroadcastManager;
import android.support.v7.app.AppCompatActivity;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.widget.Toast;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.utils.DirectoryStateReceiver;

public final class SettingsActivity extends AppCompatActivity implements SettingsActivityView
{
  private static final String ARG_MENU_TAG = "menu_tag";
  private static final String ARG_GAME_ID = "game_id";
  private static final String FRAGMENT_TAG = "settings";
  private SettingsActivityPresenter mPresenter = new SettingsActivityPresenter(this);

  private ProgressDialog dialog;

  public static void launch(Context context, MenuTag menuTag, String gameId)
  {
    Intent settings = new Intent(context, SettingsActivity.class);
    settings.putExtra(ARG_MENU_TAG, menuTag);
    settings.putExtra(ARG_GAME_ID, gameId);
    context.startActivity(settings);
  }

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);

    setContentView(R.layout.activity_settings);

    Intent launcher = getIntent();
    String gameID = launcher.getStringExtra(ARG_GAME_ID);
    MenuTag menuTag = (MenuTag) launcher.getSerializableExtra(ARG_MENU_TAG);
    mPresenter.onCreate(savedInstanceState, menuTag, gameID);
  }

  @Override
  public boolean onCreateOptionsMenu(Menu menu)
  {
    MenuInflater inflater = getMenuInflater();
    inflater.inflate(R.menu.menu_settings, menu);

    return true;
  }

  @Override
  public boolean onOptionsItemSelected(MenuItem item)
  {
    return mPresenter.handleOptionsItem(item.getItemId());
  }

  @Override
  protected void onSaveInstanceState(Bundle outState)
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
   * home button) and will expect their changes to be persisted. So we kick off an
   * IntentService which will do so on a background thread.
   */
  @Override
  protected void onStop()
  {
    super.onStop();

    mPresenter.onStop(isFinishing());
  }

  @Override
  public void onBackPressed()
  {
    mPresenter.onBackPressed();
  }


  @Override
  public void showSettingsFragment(MenuTag menuTag, Bundle extras, boolean addToStack,
          String gameID)
  {
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
      mPresenter.addToStack();
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
  public void startDirectoryInitializationService(DirectoryStateReceiver receiver,
          IntentFilter filter)
  {
    LocalBroadcastManager.getInstance(this).registerReceiver(
            receiver,
            filter);
    DirectoryInitialization.start(this);
  }

  @Override
  public void stopListeningToDirectoryInitializationService(DirectoryStateReceiver receiver)
  {
    LocalBroadcastManager.getInstance(this).unregisterReceiver(receiver);
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
  public void showPermissionNeededHint()
  {
    Toast.makeText(this, R.string.write_permission_needed, Toast.LENGTH_SHORT)
            .show();
  }

  @Override
  public void showExternalStorageNotMountedHint()
  {
    Toast.makeText(this, R.string.external_storage_not_mounted, Toast.LENGTH_SHORT)
            .show();
  }

  @Override
  public void showGameIniJunkDeletionQuestion()
  {
    new AlertDialog.Builder(this)
            .setTitle(getString(R.string.game_ini_junk_title))
            .setMessage(getString(R.string.game_ini_junk_question))
            .setPositiveButton(R.string.yes, (dialogInterface, i) -> mPresenter.clearSettings())
            .setNegativeButton(R.string.no, null)
            .create()
            .show();
  }

  @Override
  public org.dolphinemu.dolphinemu.features.settings.model.Settings getSettings()
  {
    return mPresenter.getSettings();
  }

  @Override
  public void setSettings(org.dolphinemu.dolphinemu.features.settings.model.Settings settings)
  {
    mPresenter.setSettings(settings);
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
  public void popBackStack()
  {
    getSupportFragmentManager().popBackStackImmediate();
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

  private SettingsFragment getFragment()
  {
    return (SettingsFragment) getSupportFragmentManager().findFragmentByTag(FRAGMENT_TAG);
  }
}
