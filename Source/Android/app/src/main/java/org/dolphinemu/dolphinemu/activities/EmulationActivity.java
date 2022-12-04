// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.activities;

import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.util.SparseIntArray;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.PopupMenu;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.preference.PreferenceManager;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.databinding.ActivityEmulationBinding;
import org.dolphinemu.dolphinemu.databinding.DialogInputAdjustBinding;
import org.dolphinemu.dolphinemu.databinding.DialogIrSensitivityBinding;
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivity;
import org.dolphinemu.dolphinemu.features.settings.utils.SettingsFile;
import org.dolphinemu.dolphinemu.fragments.EmulationFragment;
import org.dolphinemu.dolphinemu.fragments.MenuFragment;
import org.dolphinemu.dolphinemu.fragments.SaveLoadStateFragment;
import org.dolphinemu.dolphinemu.overlay.InputOverlay;
import org.dolphinemu.dolphinemu.overlay.InputOverlayPointer;
import org.dolphinemu.dolphinemu.ui.main.MainPresenter;
import org.dolphinemu.dolphinemu.ui.main.ThemeProvider;
import org.dolphinemu.dolphinemu.utils.AfterDirectoryInitializationRunner;
import org.dolphinemu.dolphinemu.utils.ControllerMappingHelper;
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper;
import org.dolphinemu.dolphinemu.utils.IniFile;
import org.dolphinemu.dolphinemu.utils.MotionListener;
import org.dolphinemu.dolphinemu.utils.Rumble;
import org.dolphinemu.dolphinemu.utils.ThemeHelper;

import java.io.File;
import java.lang.annotation.Retention;
import java.util.List;

import static java.lang.annotation.RetentionPolicy.SOURCE;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.android.material.slider.Slider;

public final class EmulationActivity extends AppCompatActivity implements ThemeProvider
{
  private static final String BACKSTACK_NAME_MENU = "menu";
  private static final String BACKSTACK_NAME_SUBMENU = "submenu";
  public static final int REQUEST_CHANGE_DISC = 1;

  private EmulationFragment mEmulationFragment;

  private SharedPreferences mPreferences;
  private MotionListener mMotionListener;

  private Settings mSettings;

  private int mThemeId;

  private boolean mMenuVisible;

  private static boolean sIgnoreLaunchRequests = false;

  private boolean activityRecreated;
  private String[] mPaths;
  private boolean mRiivolution;
  private boolean mLaunchSystemMenu;
  private static boolean sUserPausedEmulation;
  private boolean mMenuToastShown;

  public static final String EXTRA_SELECTED_GAMES = "SelectedGames";
  public static final String EXTRA_RIIVOLUTION = "Riivolution";
  public static final String EXTRA_SYSTEM_MENU = "SystemMenu";
  public static final String EXTRA_USER_PAUSED_EMULATION = "sUserPausedEmulation";
  public static final String EXTRA_MENU_TOAST_SHOWN = "MenuToastShown";

  @Retention(SOURCE)
  @IntDef(
          {MENU_ACTION_EDIT_CONTROLS_PLACEMENT, MENU_ACTION_TOGGLE_CONTROLS, MENU_ACTION_ADJUST_SCALE,
                  MENU_ACTION_CHOOSE_CONTROLLER, MENU_ACTION_REFRESH_WIIMOTES, MENU_ACTION_TAKE_SCREENSHOT,
                  MENU_ACTION_QUICK_SAVE, MENU_ACTION_QUICK_LOAD, MENU_ACTION_SAVE_ROOT,
                  MENU_ACTION_LOAD_ROOT, MENU_ACTION_SAVE_SLOT1, MENU_ACTION_SAVE_SLOT2,
                  MENU_ACTION_SAVE_SLOT3, MENU_ACTION_SAVE_SLOT4, MENU_ACTION_SAVE_SLOT5,
                  MENU_ACTION_SAVE_SLOT6, MENU_ACTION_LOAD_SLOT1, MENU_ACTION_LOAD_SLOT2,
                  MENU_ACTION_LOAD_SLOT3, MENU_ACTION_LOAD_SLOT4, MENU_ACTION_LOAD_SLOT5,
                  MENU_ACTION_LOAD_SLOT6, MENU_ACTION_EXIT, MENU_ACTION_CHANGE_DISC,
                  MENU_ACTION_RESET_OVERLAY, MENU_SET_IR_RECENTER, MENU_SET_IR_MODE,
                  MENU_SET_IR_SENSITIVITY, MENU_ACTION_CHOOSE_DOUBLETAP, MENU_ACTION_MOTION_CONTROLS,
                  MENU_ACTION_PAUSE_EMULATION, MENU_ACTION_UNPAUSE_EMULATION, MENU_ACTION_OVERLAY_CONTROLS,
                  MENU_ACTION_SETTINGS})
  public @interface MenuAction
  {
  }

  public static final int MENU_ACTION_EDIT_CONTROLS_PLACEMENT = 0;
  public static final int MENU_ACTION_TOGGLE_CONTROLS = 1;
  public static final int MENU_ACTION_ADJUST_SCALE = 2;
  public static final int MENU_ACTION_CHOOSE_CONTROLLER = 3;
  public static final int MENU_ACTION_REFRESH_WIIMOTES = 4;
  public static final int MENU_ACTION_TAKE_SCREENSHOT = 5;
  public static final int MENU_ACTION_QUICK_SAVE = 6;
  public static final int MENU_ACTION_QUICK_LOAD = 7;
  public static final int MENU_ACTION_SAVE_ROOT = 8;
  public static final int MENU_ACTION_LOAD_ROOT = 9;
  public static final int MENU_ACTION_SAVE_SLOT1 = 10;
  public static final int MENU_ACTION_SAVE_SLOT2 = 11;
  public static final int MENU_ACTION_SAVE_SLOT3 = 12;
  public static final int MENU_ACTION_SAVE_SLOT4 = 13;
  public static final int MENU_ACTION_SAVE_SLOT5 = 14;
  public static final int MENU_ACTION_SAVE_SLOT6 = 15;
  public static final int MENU_ACTION_LOAD_SLOT1 = 16;
  public static final int MENU_ACTION_LOAD_SLOT2 = 17;
  public static final int MENU_ACTION_LOAD_SLOT3 = 18;
  public static final int MENU_ACTION_LOAD_SLOT4 = 19;
  public static final int MENU_ACTION_LOAD_SLOT5 = 20;
  public static final int MENU_ACTION_LOAD_SLOT6 = 21;
  public static final int MENU_ACTION_EXIT = 22;
  public static final int MENU_ACTION_CHANGE_DISC = 23;
  public static final int MENU_ACTION_JOYSTICK_REL_CENTER = 24;
  public static final int MENU_ACTION_RUMBLE = 25;
  public static final int MENU_ACTION_RESET_OVERLAY = 26;
  public static final int MENU_SET_IR_RECENTER = 27;
  public static final int MENU_SET_IR_MODE = 28;
  public static final int MENU_SET_IR_SENSITIVITY = 29;
  public static final int MENU_ACTION_CHOOSE_DOUBLETAP = 30;
  public static final int MENU_ACTION_MOTION_CONTROLS = 31;
  public static final int MENU_ACTION_PAUSE_EMULATION = 32;
  public static final int MENU_ACTION_UNPAUSE_EMULATION = 33;
  public static final int MENU_ACTION_OVERLAY_CONTROLS = 34;
  public static final int MENU_ACTION_SETTINGS = 35;

  private static final SparseIntArray buttonsActionsMap = new SparseIntArray();

  static
  {
    buttonsActionsMap.append(R.id.menu_emulation_edit_layout,
            EmulationActivity.MENU_ACTION_EDIT_CONTROLS_PLACEMENT);
    buttonsActionsMap.append(R.id.menu_emulation_toggle_controls,
            EmulationActivity.MENU_ACTION_TOGGLE_CONTROLS);
    buttonsActionsMap
            .append(R.id.menu_emulation_adjust_scale, EmulationActivity.MENU_ACTION_ADJUST_SCALE);
    buttonsActionsMap.append(R.id.menu_emulation_choose_controller,
            EmulationActivity.MENU_ACTION_CHOOSE_CONTROLLER);
    buttonsActionsMap.append(R.id.menu_emulation_joystick_rel_center,
            EmulationActivity.MENU_ACTION_JOYSTICK_REL_CENTER);
    buttonsActionsMap.append(R.id.menu_emulation_rumble, EmulationActivity.MENU_ACTION_RUMBLE);
    buttonsActionsMap
            .append(R.id.menu_emulation_reset_overlay, EmulationActivity.MENU_ACTION_RESET_OVERLAY);
    buttonsActionsMap.append(R.id.menu_emulation_ir_recenter,
            EmulationActivity.MENU_SET_IR_RECENTER);
    buttonsActionsMap.append(R.id.menu_emulation_set_ir_mode,
            EmulationActivity.MENU_SET_IR_MODE);
    buttonsActionsMap.append(R.id.menu_emulation_set_ir_sensitivity,
            EmulationActivity.MENU_SET_IR_SENSITIVITY);
    buttonsActionsMap.append(R.id.menu_emulation_choose_doubletap,
            EmulationActivity.MENU_ACTION_CHOOSE_DOUBLETAP);
    buttonsActionsMap.append(R.id.menu_emulation_motion_controls,
            EmulationActivity.MENU_ACTION_MOTION_CONTROLS);
  }

  public static void launch(FragmentActivity activity, String filePath, boolean riivolution)
  {
    launch(activity, new String[]{filePath}, riivolution);
  }

  private static void performLaunchChecks(FragmentActivity activity,
          Runnable continueCallback)
  {
    new AfterDirectoryInitializationRunner().runWithLifecycle(activity, () ->
    {
      if (!FileBrowserHelper.isPathEmptyOrValid(StringSetting.MAIN_DEFAULT_ISO) ||
              !FileBrowserHelper.isPathEmptyOrValid(StringSetting.MAIN_FS_PATH) ||
              !FileBrowserHelper.isPathEmptyOrValid(StringSetting.MAIN_DUMP_PATH) ||
              !FileBrowserHelper.isPathEmptyOrValid(StringSetting.MAIN_LOAD_PATH) ||
              !FileBrowserHelper.isPathEmptyOrValid(StringSetting.MAIN_RESOURCEPACK_PATH))
      {
        new MaterialAlertDialogBuilder(activity)
                .setMessage(R.string.unavailable_paths)
                .setPositiveButton(R.string.yes,
                        (dialogInterface, i) -> SettingsActivity.launch(activity,
                                MenuTag.CONFIG_PATHS))
                .setNeutralButton(R.string.continue_anyway,
                        (dialogInterface, i) -> continueCallback.run())
                .show();
      }
      else if (!FileBrowserHelper.isPathEmptyOrValid(StringSetting.MAIN_WII_SD_CARD_IMAGE_PATH) ||
              !FileBrowserHelper.isPathEmptyOrValid(
                      StringSetting.MAIN_WII_SD_CARD_SYNC_FOLDER_PATH))
      {
        new MaterialAlertDialogBuilder(activity)
                .setMessage(R.string.unavailable_paths)
                .setPositiveButton(R.string.yes,
                        (dialogInterface, i) -> SettingsActivity.launch(activity,
                                MenuTag.CONFIG_WII))
                .setNeutralButton(R.string.continue_anyway,
                        (dialogInterface, i) -> continueCallback.run())
                .show();
      }
      else
      {
        continueCallback.run();
      }
    });
  }

  public static void launchSystemMenu(FragmentActivity activity)
  {
    if (sIgnoreLaunchRequests)
      return;

    performLaunchChecks(activity, () ->
    {
      launchSystemMenuWithoutChecks(activity);
    });
  }

  public static void launch(FragmentActivity activity, String[] filePaths, boolean riivolution)
  {
    if (sIgnoreLaunchRequests)
      return;

    performLaunchChecks(activity, () ->
    {
      launchWithoutChecks(activity, filePaths, riivolution);
    });
  }

  private static void launchSystemMenuWithoutChecks(FragmentActivity activity)
  {
    sIgnoreLaunchRequests = true;

    Intent launcher = new Intent(activity, EmulationActivity.class);
    launcher.putExtra(EmulationActivity.EXTRA_SYSTEM_MENU, true);
    activity.startActivity(launcher);
  }

  private static void launchWithoutChecks(FragmentActivity activity, String[] filePaths,
          boolean riivolution)
  {
    sIgnoreLaunchRequests = true;

    Intent launcher = new Intent(activity, EmulationActivity.class);
    launcher.putExtra(EXTRA_SELECTED_GAMES, filePaths);
    launcher.putExtra(EXTRA_RIIVOLUTION, riivolution);

    activity.startActivity(launcher);
  }

  public static void stopIgnoringLaunchRequests()
  {
    sIgnoreLaunchRequests = false;
  }

  public static void updateWiimoteNewIniPreferences(Context context)
  {
    updateWiimoteNewController(InputOverlay.getConfiguredControllerType(context), context);
    updateWiimoteNewImuIr(IntSetting.MAIN_MOTION_CONTROLS.getIntGlobal());
  }

  private static void updateWiimoteNewController(int value, Context context)
  {
    File wiimoteNewFile = SettingsFile.getSettingsFile(Settings.FILE_WIIMOTE);
    IniFile wiimoteNewIni = new IniFile(wiimoteNewFile);
    wiimoteNewIni.setString("Wiimote1", "Extension",
            context.getResources().getStringArray(R.array.controllersValues)[value]);
    wiimoteNewIni.setBoolean("Wiimote1", "Options/Sideways Wiimote", value == 2);
    wiimoteNewIni.save(wiimoteNewFile);
  }

  private static void updateWiimoteNewImuIr(int value)
  {
    File wiimoteNewFile = SettingsFile.getSettingsFile(Settings.FILE_WIIMOTE);
    IniFile wiimoteNewIni = new IniFile(wiimoteNewFile);
    wiimoteNewIni.setBoolean("Wiimote1", "IMUIR/Enabled", value != 1);
    wiimoteNewIni.save(wiimoteNewFile);
  }

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    ThemeHelper.setTheme(this);

    super.onCreate(savedInstanceState);

    MainPresenter.skipRescanningLibrary();

    if (savedInstanceState == null)
    {
      // Get params we were passed
      Intent gameToEmulate = getIntent();
      mPaths = gameToEmulate.getStringArrayExtra(EXTRA_SELECTED_GAMES);
      mRiivolution = gameToEmulate.getBooleanExtra(EXTRA_RIIVOLUTION, false);
      mLaunchSystemMenu = gameToEmulate.getBooleanExtra(EXTRA_SYSTEM_MENU, false);
      sUserPausedEmulation = gameToEmulate.getBooleanExtra(EXTRA_USER_PAUSED_EMULATION, false);
      mMenuToastShown = false;
      activityRecreated = false;
    }
    else
    {
      activityRecreated = true;
      restoreState(savedInstanceState);
    }

    mPreferences = PreferenceManager.getDefaultSharedPreferences(this);

    mSettings = new Settings();
    mSettings.loadSettings();

    updateOrientation();

    mMotionListener = new MotionListener(this);

    // Set these options now so that the SurfaceView the game renders into is the right size.
    enableFullscreenImmersive();

    Rumble.initRumble(this);

    ActivityEmulationBinding binding = ActivityEmulationBinding.inflate(getLayoutInflater());
    setContentView(binding.getRoot());

    setInsets(binding.frameMenu);

    // Find or create the EmulationFragment
    mEmulationFragment = (EmulationFragment) getSupportFragmentManager()
            .findFragmentById(R.id.frame_emulation_fragment);
    if (mEmulationFragment == null)
    {
      mEmulationFragment = EmulationFragment.newInstance(mPaths, mRiivolution, mLaunchSystemMenu);
      getSupportFragmentManager().beginTransaction()
              .add(R.id.frame_emulation_fragment, mEmulationFragment)
              .commit();
    }

    if (NativeLibrary.IsGameMetadataValid())
      setTitle(NativeLibrary.GetCurrentTitleDescription());
  }

  @Override
  protected void onSaveInstanceState(@NonNull Bundle outState)
  {
    if (!isChangingConfigurations())
    {
      mEmulationFragment.saveTemporaryState();
    }
    outState.putStringArray(EXTRA_SELECTED_GAMES, mPaths);
    outState.putBoolean(EXTRA_USER_PAUSED_EMULATION, sUserPausedEmulation);
    outState.putBoolean(EXTRA_MENU_TOAST_SHOWN, mMenuToastShown);
    super.onSaveInstanceState(outState);
  }

  protected void restoreState(Bundle savedInstanceState)
  {
    mPaths = savedInstanceState.getStringArray(EXTRA_SELECTED_GAMES);
    sUserPausedEmulation = savedInstanceState.getBoolean(EXTRA_USER_PAUSED_EMULATION);
    mMenuToastShown = savedInstanceState.getBoolean(EXTRA_MENU_TOAST_SHOWN);
  }

  @Override
  public void onWindowFocusChanged(boolean hasFocus)
  {
    if (hasFocus)
    {
      enableFullscreenImmersive();
    }
  }

  @Override
  protected void onResume()
  {
    ThemeHelper.setCorrectTheme(this);

    super.onResume();

    // Only android 9+ support this feature.
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P)
    {
      WindowManager.LayoutParams attributes = getWindow().getAttributes();

      attributes.layoutInDisplayCutoutMode =
              BooleanSetting.MAIN_EXPAND_TO_CUTOUT_AREA.getBoolean(mSettings) ?
                      WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES :
                      WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER;

      getWindow().setAttributes(attributes);
    }

    updateOrientation();

    if (NativeLibrary.IsGameMetadataValid())
      updateMotionListener();
  }

  @Override
  protected void onPause()
  {
    super.onPause();
    mMotionListener.disable();
  }

  @Override
  protected void onStop()
  {
    super.onStop();
    mSettings.saveSettings(null, null);
  }

  public void onTitleChanged()
  {
    if (!mMenuToastShown)
    {
      // The reason why this doesn't run earlier is because we want to be sure the boot succeeded.
      Toast.makeText(this, R.string.emulation_menu_help, Toast.LENGTH_LONG).show();
      mMenuToastShown = true;
    }

    setTitle(NativeLibrary.GetCurrentTitleDescription());
    updateMotionListener();

    mEmulationFragment.refreshInputOverlay();
  }

  private void updateMotionListener()
  {
    if (NativeLibrary.IsEmulatingWii() && IntSetting.MAIN_MOTION_CONTROLS.getInt(mSettings) != 2)
      mMotionListener.enable();
    else
      mMotionListener.disable();
  }

  @Override
  protected void onDestroy()
  {
    super.onDestroy();
    mSettings.close();
  }

  @Override
  public void onBackPressed()
  {
    if (!closeSubmenu())
    {
      toggleMenu();
    }
  }

  @Override
  public boolean onKeyLongPress(int keyCode, @NonNull KeyEvent event)
  {
    if (keyCode == KeyEvent.KEYCODE_BACK)
    {
      mEmulationFragment.stopEmulation();
      return true;
    }
    return super.onKeyLongPress(keyCode, event);
  }

  @Override
  protected void onActivityResult(int requestCode, int resultCode, Intent result)
  {
    super.onActivityResult(requestCode, resultCode, result);
    if (requestCode == REQUEST_CHANGE_DISC)
    {
      // If the user picked a file, as opposed to just backing out.
      if (resultCode == RESULT_OK)
      {
        NativeLibrary.ChangeDisc(result.getData().toString());
      }
    }
  }

  private void enableFullscreenImmersive()
  {
    getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
                    View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                    View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                    View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                    View.SYSTEM_UI_FLAG_FULLSCREEN |
                    View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
  }

  private void updateOrientation()
  {
    setRequestedOrientation(IntSetting.MAIN_EMULATION_ORIENTATION.getInt(mSettings));
  }

  private boolean closeSubmenu()
  {
    return getSupportFragmentManager().popBackStackImmediate(BACKSTACK_NAME_SUBMENU,
            FragmentManager.POP_BACK_STACK_INCLUSIVE);
  }

  private boolean closeMenu()
  {
    mMenuVisible = false;
    return getSupportFragmentManager().popBackStackImmediate(BACKSTACK_NAME_MENU,
            FragmentManager.POP_BACK_STACK_INCLUSIVE);
  }

  private void toggleMenu()
  {
    if (!closeMenu())
    {
      // Removing the menu failed, so that means it wasn't visible. Add it.
      Fragment fragment = MenuFragment.newInstance();
      getSupportFragmentManager().beginTransaction()
              .setCustomAnimations(
                      R.animator.menu_slide_in_from_start,
                      R.animator.menu_slide_out_to_start,
                      R.animator.menu_slide_in_from_start,
                      R.animator.menu_slide_out_to_start)
              .add(R.id.frame_menu, fragment)
              .addToBackStack(BACKSTACK_NAME_MENU)
              .commit();
      mMenuVisible = true;
    }
  }

  private void setInsets(View view)
  {
    ViewCompat.setOnApplyWindowInsetsListener(view, (v, windowInsets) ->
    {
      Insets cutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout());
      ViewGroup.MarginLayoutParams mlpMenu =
              (ViewGroup.MarginLayoutParams) v.getLayoutParams();
      int menuWidth = getResources().getDimensionPixelSize(R.dimen.menu_width);
      if (ViewCompat.getLayoutDirection(v) == ViewCompat.LAYOUT_DIRECTION_LTR)
      {
        mlpMenu.width = cutInsets.left + menuWidth;
      }
      else
      {
        mlpMenu.width = cutInsets.right + menuWidth;
      }
      NativeLibrary.SetObscuredPixelsTop(cutInsets.top);
      NativeLibrary.SetObscuredPixelsLeft(cutInsets.left);
      return windowInsets;
    });
  }

  public void showOverlayControlsMenu(@NonNull View anchor)
  {
    PopupMenu popup = new PopupMenu(this, anchor);
    Menu menu = popup.getMenu();

    boolean wii = NativeLibrary.IsEmulatingWii();
    int id = wii ? R.menu.menu_overlay_controls_wii : R.menu.menu_overlay_controls_gc;
    popup.getMenuInflater().inflate(id, menu);

    // Populate the checkbox value for joystick center on touch
    menu.findItem(R.id.menu_emulation_joystick_rel_center)
            .setChecked(BooleanSetting.MAIN_JOYSTICK_REL_CENTER.getBoolean(mSettings));
    menu.findItem(R.id.menu_emulation_rumble)
            .setChecked(BooleanSetting.MAIN_PHONE_RUMBLE.getBoolean(mSettings));
    if (wii)
    {
      menu.findItem(R.id.menu_emulation_ir_recenter)
              .setChecked(BooleanSetting.MAIN_IR_ALWAYS_RECENTER.getBoolean(mSettings));
    }

    popup.setOnMenuItemClickListener(this::onOptionsItemSelected);

    popup.show();
  }

  @Override
  public boolean onOptionsItemSelected(MenuItem item)
  {
    int action = buttonsActionsMap.get(item.getItemId(), -1);
    if (action >= 0)
    {
      if (item.isCheckable())
      {
        // Need to pass a reference to the item to set the checkbox state, since it is not done automatically
        handleCheckableMenuAction(action, item);
      }
      else
      {
        handleMenuAction(action);
      }
    }
    return true;
  }

  public void handleCheckableMenuAction(@MenuAction int menuAction, MenuItem item)
  {
    //noinspection SwitchIntDef
    switch (menuAction)
    {
      case MENU_ACTION_JOYSTICK_REL_CENTER:
        item.setChecked(!item.isChecked());
        toggleJoystickRelCenter(item.isChecked());
        break;
      case MENU_ACTION_RUMBLE:
        item.setChecked(!item.isChecked());
        toggleRumble(item.isChecked());
        break;
      case MENU_SET_IR_RECENTER:
        item.setChecked(!item.isChecked());
        toggleRecenter(item.isChecked());
        break;
    }
  }

  public void handleMenuAction(@MenuAction int menuAction)
  {
    //noinspection SwitchIntDef
    switch (menuAction)
    {
      // Edit the placement of the controls
      case MENU_ACTION_EDIT_CONTROLS_PLACEMENT:
        editControlsPlacement();
        break;

      // Reset overlay placement
      case MENU_ACTION_RESET_OVERLAY:
        resetOverlay();
        break;

      // Enable/Disable specific buttons or the entire input overlay.
      case MENU_ACTION_TOGGLE_CONTROLS:
        toggleControls();
        break;

      // Adjust the scale and opacity of the overlay controls.
      case MENU_ACTION_ADJUST_SCALE:
        adjustScale();
        break;

      // (Wii games only) Change the controller for the input overlay.
      case MENU_ACTION_CHOOSE_CONTROLLER:
        chooseController();
        break;

      case MENU_ACTION_REFRESH_WIIMOTES:
        NativeLibrary.RefreshWiimotes();
        break;

      case MENU_ACTION_PAUSE_EMULATION:
        sUserPausedEmulation = true;
        NativeLibrary.PauseEmulation();
        break;

      case MENU_ACTION_UNPAUSE_EMULATION:
        sUserPausedEmulation = false;
        NativeLibrary.UnPauseEmulation();
        break;

      // Screenshot capturing
      case MENU_ACTION_TAKE_SCREENSHOT:
        NativeLibrary.SaveScreenShot();
        break;

      // Quick save / load
      case MENU_ACTION_QUICK_SAVE:
        NativeLibrary.SaveState(9, false);
        break;

      case MENU_ACTION_QUICK_LOAD:
        NativeLibrary.LoadState(9);
        break;

      case MENU_ACTION_SAVE_ROOT:
        showSubMenu(SaveLoadStateFragment.SaveOrLoad.SAVE);
        break;

      case MENU_ACTION_LOAD_ROOT:
        showSubMenu(SaveLoadStateFragment.SaveOrLoad.LOAD);
        break;

      // Save state slots
      case MENU_ACTION_SAVE_SLOT1:
        NativeLibrary.SaveState(0, false);
        break;

      case MENU_ACTION_SAVE_SLOT2:
        NativeLibrary.SaveState(1, false);
        break;

      case MENU_ACTION_SAVE_SLOT3:
        NativeLibrary.SaveState(2, false);
        break;

      case MENU_ACTION_SAVE_SLOT4:
        NativeLibrary.SaveState(3, false);
        break;

      case MENU_ACTION_SAVE_SLOT5:
        NativeLibrary.SaveState(4, false);
        break;

      case MENU_ACTION_SAVE_SLOT6:
        NativeLibrary.SaveState(5, false);
        break;

      // Load state slots
      case MENU_ACTION_LOAD_SLOT1:
        NativeLibrary.LoadState(0);
        break;

      case MENU_ACTION_LOAD_SLOT2:
        NativeLibrary.LoadState(1);
        break;

      case MENU_ACTION_LOAD_SLOT3:
        NativeLibrary.LoadState(2);
        break;

      case MENU_ACTION_LOAD_SLOT4:
        NativeLibrary.LoadState(3);
        break;

      case MENU_ACTION_LOAD_SLOT5:
        NativeLibrary.LoadState(4);
        break;

      case MENU_ACTION_LOAD_SLOT6:
        NativeLibrary.LoadState(5);
        break;

      case MENU_ACTION_CHANGE_DISC:
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        startActivityForResult(intent, REQUEST_CHANGE_DISC);
        break;

      case MENU_SET_IR_MODE:
        setIRMode();
        break;

      case MENU_SET_IR_SENSITIVITY:
        setIRSensitivity();
        break;

      case MENU_ACTION_CHOOSE_DOUBLETAP:
        chooseDoubleTapButton();
        break;

      case MENU_ACTION_MOTION_CONTROLS:
        showMotionControlsOptions();
        break;

      case MENU_ACTION_SETTINGS:
        SettingsActivity.launch(this, MenuTag.SETTINGS);
        break;

      case MENU_ACTION_EXIT:
        mEmulationFragment.stopEmulation();
        break;
    }
  }

  public static boolean getHasUserPausedEmulation()
  {
    return sUserPausedEmulation;
  }

  private void toggleJoystickRelCenter(boolean state)
  {
    BooleanSetting.MAIN_JOYSTICK_REL_CENTER.setBoolean(mSettings, state);
  }

  private void toggleRumble(boolean state)
  {
    BooleanSetting.MAIN_PHONE_RUMBLE.setBoolean(mSettings, state);
    Rumble.setPhoneVibrator(state, this);
  }

  private void toggleRecenter(boolean state)
  {
    BooleanSetting.MAIN_IR_ALWAYS_RECENTER.setBoolean(mSettings, state);
    mEmulationFragment.refreshOverlayPointer(mSettings);
  }

  private void editControlsPlacement()
  {
    if (mEmulationFragment.isConfiguringControls())
    {
      mEmulationFragment.stopConfiguringControls();
    }
    else
    {
      closeSubmenu();
      closeMenu();
      mEmulationFragment.startConfiguringControls();
    }
  }

  // Gets button presses
  @Override
  public boolean dispatchKeyEvent(KeyEvent event)
  {
    if (mMenuVisible || event.getKeyCode() == KeyEvent.KEYCODE_BACK)
    {
      return super.dispatchKeyEvent(event);
    }

    int action;

    switch (event.getAction())
    {
      case KeyEvent.ACTION_DOWN:
        action = NativeLibrary.ButtonState.PRESSED;
        break;
      case KeyEvent.ACTION_UP:
        action = NativeLibrary.ButtonState.RELEASED;
        break;
      default:
        return false;
    }
    InputDevice input = event.getDevice();
    return NativeLibrary.onGamePadEvent(input.getDescriptor(), event.getKeyCode(), action);
  }

  private void toggleControls()
  {
    MaterialAlertDialogBuilder builder = new MaterialAlertDialogBuilder(this)
            .setTitle(R.string.emulation_toggle_controls);

    int currentController = InputOverlay.getConfiguredControllerType(this);

    if (!NativeLibrary.IsEmulatingWii() || currentController == InputOverlay.OVERLAY_GAMECUBE)
    {
      boolean[] gcEnabledButtons = new boolean[11];
      String gcSettingBase = "MAIN_BUTTON_TOGGLE_GC_";

      for (int i = 0; i < gcEnabledButtons.length; i++)
      {
        gcEnabledButtons[i] = BooleanSetting.valueOf(gcSettingBase + i).getBoolean(mSettings);
      }
      builder.setMultiChoiceItems(R.array.gcpadButtons, gcEnabledButtons,
              (dialog, indexSelected, isChecked) -> BooleanSetting
                      .valueOf(gcSettingBase + indexSelected).setBoolean(mSettings, isChecked));
    }
    else if (currentController == InputOverlay.OVERLAY_WIIMOTE_CLASSIC)
    {
      boolean[] wiiClassicEnabledButtons = new boolean[14];
      String classicSettingBase = "MAIN_BUTTON_TOGGLE_CLASSIC_";

      for (int i = 0; i < wiiClassicEnabledButtons.length; i++)
      {
        wiiClassicEnabledButtons[i] =
                BooleanSetting.valueOf(classicSettingBase + i).getBoolean(mSettings);
      }
      builder.setMultiChoiceItems(R.array.classicButtons, wiiClassicEnabledButtons,
              (dialog, indexSelected, isChecked) -> BooleanSetting
                      .valueOf(classicSettingBase + indexSelected)
                      .setBoolean(mSettings, isChecked));
    }
    else
    {
      boolean[] wiiEnabledButtons = new boolean[11];
      String wiiSettingBase = "MAIN_BUTTON_TOGGLE_WII_";

      for (int i = 0; i < wiiEnabledButtons.length; i++)
      {
        wiiEnabledButtons[i] = BooleanSetting.valueOf(wiiSettingBase + i).getBoolean(mSettings);
      }
      if (currentController == InputOverlay.OVERLAY_WIIMOTE_NUNCHUK)
      {
        builder.setMultiChoiceItems(R.array.nunchukButtons, wiiEnabledButtons,
                (dialog, indexSelected, isChecked) -> BooleanSetting
                        .valueOf(wiiSettingBase + indexSelected).setBoolean(mSettings, isChecked));
      }
      else
      {
        builder.setMultiChoiceItems(R.array.wiimoteButtons, wiiEnabledButtons,
                (dialog, indexSelected, isChecked) -> BooleanSetting
                        .valueOf(wiiSettingBase + indexSelected).setBoolean(mSettings, isChecked));
      }
    }

    builder.setNeutralButton(R.string.emulation_toggle_all,
                    (dialogInterface, i) -> mEmulationFragment.toggleInputOverlayVisibility(mSettings))
            .setPositiveButton(R.string.ok, (dialogInterface, i) ->
                    mEmulationFragment.refreshInputOverlay())
            .show();
  }

  public void chooseDoubleTapButton()
  {
    int currentValue = IntSetting.MAIN_DOUBLE_TAP_BUTTON.getInt(mSettings);

    int buttonList = InputOverlay.getConfiguredControllerType(this) ==
            InputOverlay.OVERLAY_WIIMOTE_CLASSIC ? R.array.doubleTapWithClassic : R.array.doubleTap;

    int checkedItem = -1;
    int itemCount = getResources().getStringArray(buttonList).length;
    for (int i = 0; i < itemCount; i++)
    {
      if (InputOverlayPointer.DOUBLE_TAP_OPTIONS.get(i) == currentValue)
      {
        checkedItem = i;
        break;
      }
    }

    new MaterialAlertDialogBuilder(this)
            .setSingleChoiceItems(buttonList, checkedItem,
                    (DialogInterface dialog, int which) -> IntSetting.MAIN_DOUBLE_TAP_BUTTON.setInt(
                            mSettings, InputOverlayPointer.DOUBLE_TAP_OPTIONS.get(which)))
            .setPositiveButton(R.string.ok,
                    (dialogInterface, i) -> mEmulationFragment.initInputPointer())
            .show();
  }

  private void adjustScale()
  {
    DialogInputAdjustBinding dialogBinding = DialogInputAdjustBinding.inflate(getLayoutInflater());

    final Slider scaleSlider = dialogBinding.inputScaleSlider;
    final TextView scaleValue = dialogBinding.inputScaleValue;
    scaleSlider.setValueTo(150);
    scaleSlider.setValue(IntSetting.MAIN_CONTROL_SCALE.getInt(mSettings));
    scaleSlider.setStepSize(1);
    scaleSlider.addOnChangeListener(
            (slider, progress, fromUser) -> scaleValue.setText(((int) progress + 50) + "%"));
    scaleValue.setText(((int) scaleSlider.getValue() + 50) + "%");

    // alpha
    final Slider sliderOpacity = dialogBinding.inputOpacitySlider;
    final TextView valueOpacity = dialogBinding.inputOpacityValue;
    sliderOpacity.setValueTo(100);
    sliderOpacity.setValue(IntSetting.MAIN_CONTROL_OPACITY.getInt(mSettings));
    sliderOpacity.setStepSize(1);
    sliderOpacity.addOnChangeListener(
            (slider, progress, fromUser) -> valueOpacity.setText(((int) progress) + "%"));
    valueOpacity.setText(((int) sliderOpacity.getValue()) + "%");

    new MaterialAlertDialogBuilder(this)
            .setTitle(R.string.emulation_control_adjustments)
            .setView(dialogBinding.getRoot())
            .setPositiveButton(R.string.ok, (dialog, which) ->
            {
              IntSetting.MAIN_CONTROL_SCALE.setInt(mSettings, (int) scaleSlider.getValue());
              IntSetting.MAIN_CONTROL_OPACITY.setInt(mSettings, (int) sliderOpacity.getValue());
              mEmulationFragment.refreshInputOverlay();
            })
            .setNeutralButton(R.string.default_values, (dialog, which) ->
            {
              IntSetting.MAIN_CONTROL_SCALE.delete(mSettings);
              IntSetting.MAIN_CONTROL_OPACITY.delete(mSettings);
              mEmulationFragment.refreshInputOverlay();
            })
            .show();
  }

  private void chooseController()
  {
    final SharedPreferences.Editor editor = mPreferences.edit();
    new MaterialAlertDialogBuilder(this)
            .setTitle(R.string.emulation_choose_controller)
            .setSingleChoiceItems(R.array.controllersEntries,
                    InputOverlay.getConfiguredControllerType(this),
                    (dialog, indexSelected) ->
                    {
                      editor.putInt("wiiController", indexSelected);

                      updateWiimoteNewController(indexSelected, this);
                      NativeLibrary.ReloadWiimoteConfig();
                    })
            .setPositiveButton(R.string.ok, (dialogInterface, i) ->
            {
              editor.apply();
              mEmulationFragment.refreshInputOverlay();
            })
            .show();
  }

  private void showMotionControlsOptions()
  {
    new MaterialAlertDialogBuilder(this)
            .setTitle(R.string.emulation_motion_controls)
            .setSingleChoiceItems(R.array.motionControlsEntries,
                    IntSetting.MAIN_MOTION_CONTROLS.getInt(mSettings),
                    (dialog, indexSelected) ->
                    {
                      IntSetting.MAIN_MOTION_CONTROLS.setInt(mSettings, indexSelected);

                      updateMotionListener();

                      updateWiimoteNewImuIr(indexSelected);
                      NativeLibrary.ReloadWiimoteConfig();
                    })
            .setPositiveButton(R.string.ok, (dialogInterface, i) -> dialogInterface.dismiss())
            .show();
  }

  private void setIRMode()
  {
    new MaterialAlertDialogBuilder(this)
            .setTitle(R.string.emulation_ir_mode)
            .setSingleChoiceItems(R.array.irModeEntries,
                    IntSetting.MAIN_IR_MODE.getInt(mSettings),
                    (dialog, indexSelected) ->
                            IntSetting.MAIN_IR_MODE.setInt(mSettings, indexSelected))
            .setPositiveButton(R.string.ok, (dialogInterface, i) ->
                    mEmulationFragment.refreshOverlayPointer(mSettings))
            .show();
  }

  private void setIRSensitivity()
  {
    // IR settings always get saved per-game since WiimoteNew.ini is wiped upon reinstall.
    File file = SettingsFile.getCustomGameSettingsFile(NativeLibrary.GetCurrentGameID());
    IniFile ini = new IniFile(file);

    int ir_pitch = ini.getInt(Settings.SECTION_CONTROLS, SettingsFile.KEY_WIIBIND_IR_PITCH, 20);

    DialogIrSensitivityBinding dialogBinding =
            DialogIrSensitivityBinding.inflate(getLayoutInflater());

    TextView text_slider_value_pitch = dialogBinding.textIrPitch;
    TextView units = dialogBinding.textIrPitchUnits;
    Slider slider_pitch = dialogBinding.sliderPitch;

    text_slider_value_pitch.setText(String.valueOf(ir_pitch));
    units.setText(getString(R.string.pitch));
    slider_pitch.setValueTo(100);
    slider_pitch.setValue(ir_pitch);
    slider_pitch.setStepSize(1);
    slider_pitch.addOnChangeListener(
            (slider, progress, fromUser) -> text_slider_value_pitch.setText(
                    String.valueOf((int) progress)));

    int ir_yaw = ini.getInt(Settings.SECTION_CONTROLS, SettingsFile.KEY_WIIBIND_IR_YAW, 25);

    TextView text_slider_value_yaw = dialogBinding.textIrYaw;
    TextView units_yaw = dialogBinding.textIrYawUnits;
    Slider seekbar_yaw = dialogBinding.sliderYaw;

    text_slider_value_yaw.setText(String.valueOf(ir_yaw));
    units_yaw.setText(getString(R.string.yaw));
    seekbar_yaw.setValueTo(100);
    seekbar_yaw.setValue(ir_yaw);
    seekbar_yaw.setStepSize(1);
    seekbar_yaw.addOnChangeListener((slider, progress, fromUser) -> text_slider_value_yaw.setText(
            String.valueOf((int) progress)));

    int ir_vertical_offset =
            ini.getInt(Settings.SECTION_CONTROLS, SettingsFile.KEY_WIIBIND_IR_VERTICAL_OFFSET, 10);

    TextView text_slider_value_vertical_offset = dialogBinding.textIrVerticalOffset;
    TextView units_vertical_offset = dialogBinding.textIrVerticalOffsetUnits;
    Slider seekbar_vertical_offset = dialogBinding.sliderVerticalOffset;

    text_slider_value_vertical_offset.setText(String.valueOf(ir_vertical_offset));
    units_vertical_offset.setText(getString(R.string.vertical_offset));
    seekbar_vertical_offset.setValueTo(100);
    seekbar_vertical_offset.setValue(ir_vertical_offset);
    seekbar_vertical_offset.setStepSize(1);
    seekbar_vertical_offset.addOnChangeListener(
            (slider, progress, fromUser) -> text_slider_value_vertical_offset.setText(
                    String.valueOf((int) progress)));

    new MaterialAlertDialogBuilder(this)
            .setTitle(getString(R.string.emulation_ir_sensitivity))
            .setView(dialogBinding.getRoot())
            .setPositiveButton(R.string.ok, (dialogInterface, i) ->
            {
              ini.setString(Settings.SECTION_CONTROLS, SettingsFile.KEY_WIIBIND_IR_PITCH,
                      text_slider_value_pitch.getText().toString());
              ini.setString(Settings.SECTION_CONTROLS, SettingsFile.KEY_WIIBIND_IR_YAW,
                      text_slider_value_yaw.getText().toString());
              ini.setString(Settings.SECTION_CONTROLS, SettingsFile.KEY_WIIBIND_IR_VERTICAL_OFFSET,
                      text_slider_value_vertical_offset.getText().toString());
              ini.save(file);

              NativeLibrary.ReloadWiimoteConfig();
            })
            .setNegativeButton(R.string.cancel, null)
            .setNeutralButton(R.string.default_values, (dialogInterface, i) ->
            {
              ini.deleteKey(Settings.SECTION_CONTROLS, SettingsFile.KEY_WIIBIND_IR_PITCH);
              ini.deleteKey(Settings.SECTION_CONTROLS, SettingsFile.KEY_WIIBIND_IR_YAW);
              ini.deleteKey(Settings.SECTION_CONTROLS, SettingsFile.KEY_WIIBIND_IR_VERTICAL_OFFSET);
              ini.save(file);

              NativeLibrary.ReloadWiimoteConfig();
            })
            .show();
  }

  private void resetOverlay()
  {
    new MaterialAlertDialogBuilder(this)
            .setTitle(getString(R.string.emulation_touch_overlay_reset))
            .setPositiveButton(R.string.yes,
                    (dialogInterface, i) -> mEmulationFragment.resetInputOverlay())
            .setNegativeButton(R.string.cancel, null)
            .show();
  }

  private static boolean areCoordinatesOutside(@Nullable View view, float x, float y)
  {
    if (view == null)
    {
      return true;
    }

    Rect viewBounds = new Rect();
    view.getGlobalVisibleRect(viewBounds);
    return !viewBounds.contains(Math.round(x), Math.round(y));
  }

  @Override
  public boolean dispatchTouchEvent(MotionEvent event)
  {
    if (event.getActionMasked() == MotionEvent.ACTION_DOWN)
    {
      boolean anyMenuClosed = false;

      Fragment submenu = getSupportFragmentManager().findFragmentById(R.id.frame_submenu);
      if (submenu != null && areCoordinatesOutside(submenu.getView(), event.getX(), event.getY()))
      {
        closeSubmenu();
        submenu = null;
        anyMenuClosed = true;
      }

      if (submenu == null)
      {
        Fragment menu = getSupportFragmentManager().findFragmentById(R.id.frame_menu);
        if (menu != null && areCoordinatesOutside(menu.getView(), event.getX(), event.getY()))
        {
          closeMenu();
          anyMenuClosed = true;
        }
      }

      if (anyMenuClosed)
      {
        return true;
      }
    }

    return super.dispatchTouchEvent(event);
  }

  @Override
  public boolean dispatchGenericMotionEvent(MotionEvent event)
  {
    if (mMenuVisible)
    {
      return false;
    }

    if (((event.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) == 0))
    {
      return super.dispatchGenericMotionEvent(event);
    }

    // Don't attempt to do anything if we are disconnecting a device.
    if (event.getActionMasked() == MotionEvent.ACTION_CANCEL)
      return true;

    InputDevice input = event.getDevice();
    List<InputDevice.MotionRange> motions = input.getMotionRanges();

    for (InputDevice.MotionRange range : motions)
    {
      int axis = range.getAxis();
      float origValue = event.getAxisValue(axis);
      float value = ControllerMappingHelper.scaleAxis(input, axis, origValue);
      // If the input is still in the "flat" area, that means it's really zero.
      // This is used to compensate for imprecision in joysticks.
      if (Math.abs(value) > range.getFlat())
      {
        NativeLibrary.onGamePadMoveEvent(input.getDescriptor(), axis, value);
      }
      else
      {
        NativeLibrary.onGamePadMoveEvent(input.getDescriptor(), axis, 0.0f);
      }
    }

    return true;
  }

  private void showSubMenu(SaveLoadStateFragment.SaveOrLoad saveOrLoad)
  {
    // Get rid of any visible submenu
    getSupportFragmentManager().popBackStack(
            BACKSTACK_NAME_SUBMENU, FragmentManager.POP_BACK_STACK_INCLUSIVE);

    Fragment fragment = SaveLoadStateFragment.newInstance(saveOrLoad);
    getSupportFragmentManager().beginTransaction()
            .setCustomAnimations(
                    R.animator.menu_slide_in_from_end,
                    R.animator.menu_slide_out_to_end,
                    R.animator.menu_slide_in_from_end,
                    R.animator.menu_slide_out_to_end)
            .replace(R.id.frame_submenu, fragment)
            .addToBackStack(BACKSTACK_NAME_SUBMENU)
            .commit();
  }

  public boolean isActivityRecreated()
  {
    return activityRecreated;
  }

  public Settings getSettings()
  {
    return mSettings;
  }

  public void initInputPointer()
  {
    mEmulationFragment.initInputPointer();
  }

  @Override
  public void setTheme(int themeId)
  {
    super.setTheme(themeId);
    this.mThemeId = themeId;
  }

  @Override
  public int getThemeId()
  {
    return mThemeId;
  }
}
