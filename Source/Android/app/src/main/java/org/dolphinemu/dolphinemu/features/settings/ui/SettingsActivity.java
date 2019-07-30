package org.dolphinemu.dolphinemu.features.settings.ui;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.v4.app.FragmentTransaction;
import android.support.v7.app.AppCompatActivity;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.widget.Toast;

import org.dolphinemu.dolphinemu.features.settings.model.Setting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

import org.dolphinemu.dolphinemu.R;

public final class SettingsActivity extends AppCompatActivity implements SettingsActivityView
{
  public static final String ARG_CONTROLLER_TYPE = "controller_type";

  private static final String ARG_MENU_TAG = "menu_tag";
  private static final String ARG_GAME_ID = "game_id";
  private static final String FRAGMENT_TAG = "settings";

  private static final String KEY_SHOULD_SAVE = "should_save";
  private static final String KEY_MENU_TAG = "menu_tag";
  private static final String KEY_GAME_ID = "game_id";

  private Settings mSettings = new Settings();
  private int mStackCount;
  private boolean mShouldSave;
  private MenuTag mMenuTag;
  private String mGameId;

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
    String gameId = launcher.getStringExtra(ARG_GAME_ID);
    MenuTag menuTag = (MenuTag) launcher.getSerializableExtra(ARG_MENU_TAG);

    if (savedInstanceState == null)
    {
      mMenuTag = menuTag;
      mGameId = gameId;
    }
    else
    {
      String menuTagStr = savedInstanceState.getString(KEY_MENU_TAG);
      mShouldSave = savedInstanceState.getBoolean(KEY_SHOULD_SAVE);
      mMenuTag = MenuTag.getMenuTag(menuTagStr);
      mGameId = savedInstanceState.getString(KEY_GAME_ID);
    }

    if (!TextUtils.isEmpty(mGameId))
    {
      setTitle(getString(R.string.per_game_settings, mGameId));
    }
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
    switch (item.getItemId())
    {
      case R.id.menu_save_exit:
        finish();
        return true;
    }
    return false;
  }

  @Override
  protected void onSaveInstanceState(Bundle outState)
  {
    // Critical: If super method is not called, rotations will be busted.
    super.onSaveInstanceState(outState);

    outState.putBoolean(KEY_SHOULD_SAVE, mShouldSave);
    outState.putString(KEY_MENU_TAG, mMenuTag.toString());
    outState.putString(KEY_GAME_ID, mGameId);
  }

  @Override
  protected void onStart()
  {
    super.onStart();
    loadSettingsUI();
  }

  private void loadSettingsUI()
  {
    if (mSettings.isEmpty())
    {
      mSettings.loadSettings(mGameId);
    }

    showSettingsFragment(mMenuTag, null, false, mGameId);
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

    if (mSettings != null && isFinishing() && mShouldSave)
    {
      if (TextUtils.isEmpty(mGameId))
      {
        showToastMessage(getString(R.string.settings_saved_notice));
        mSettings.saveSettings();
      }
      else
      {
        // custom game settings
        showToastMessage(getString(R.string.settings_saved_notice));
        mSettings.saveCustomGameSettings(mGameId);
      }
      mShouldSave = false;
    }

    getFragment().closeDialog();
    mStackCount = 0;
  }

  @Override
  public void onBackPressed()
  {
    if (mStackCount > 0)
    {
      getSupportFragmentManager().popBackStackImmediate();
      mStackCount--;
    }
    else
    {
      finish();
    }
  }

  @Override
  public void showSettingsFragment(MenuTag menuTag, Bundle extras, boolean addToStack, String gameID)
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
      mStackCount++;
    }
    transaction.replace(R.id.frame_content, SettingsFragment.newInstance(menuTag, gameID, extras), FRAGMENT_TAG);
    transaction.commit();

    // show settings
    SettingsFragmentView fragment = getFragment();
    if (fragment != null)
    {
      fragment.showSettingsList(mSettings);
    }
  }

  private boolean areSystemAnimationsEnabled()
  {
    float duration = android.provider.Settings.Global.getFloat(
      getContentResolver(),
      android.provider.Settings.Global.ANIMATOR_DURATION_SCALE, 1);

    float transition = android.provider.Settings.Global.getFloat(
      getContentResolver(),
      android.provider.Settings.Global.TRANSITION_ANIMATION_SCALE, 1);

    return duration != 0 && transition != 0;
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

  public String getGameId()
  {
    return mGameId;
  }

  public Settings getSettings()
  {
    return mSettings;
  }

  public void setSettings(Settings settings)
  {
    mSettings = settings;
  }

  public void putSetting(Setting setting)
  {
    mSettings.getSection(setting.getSection()).putSetting(setting);
  }

  public void setSettingChanged()
  {
    mShouldSave = true;
  }

  public void loadSubMenu(MenuTag menuKey)
  {
    showSettingsFragment(menuKey, null, true, mGameId);
  }

  @Override
  public void showToastMessage(String message)
  {
    Toast.makeText(this, message, Toast.LENGTH_LONG).show();
  }

  @Override
  public void onGcPadSettingChanged(MenuTag key, int value)
  {
    if (value != 0) // Not disabled
    {
      Bundle bundle = new Bundle();
      bundle.putInt(ARG_CONTROLLER_TYPE, value / 6);
      showSettingsFragment(key, bundle, true, mGameId);
    }
  }

  @Override
  public void onWiimoteSettingChanged(MenuTag menuTag, int value)
  {
    switch (value)
    {
      case 1:
        showSettingsFragment(menuTag, null, true, mGameId);
        break;

      case 2:
        showToastMessage("Please make sure Continuous Scanning is enabled in Core Settings.");
        break;
    }
  }

  @Override
  public void onExtensionSettingChanged(MenuTag menuTag, int value)
  {
    if (value != 0) // None
    {
      Bundle bundle = new Bundle();
      bundle.putInt(ARG_CONTROLLER_TYPE, value);
      showSettingsFragment(menuTag, bundle, true, mGameId);
    }
  }

  private SettingsFragment getFragment()
  {
    return (SettingsFragment) getSupportFragmentManager().findFragmentByTag(FRAGMENT_TAG);
  }
}
