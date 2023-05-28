// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.activities;

import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.util.Pair;
import android.util.SparseIntArray;
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
import androidx.recyclerview.widget.LinearLayoutManager;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.databinding.ActivityEmulationBinding;
import org.dolphinemu.dolphinemu.databinding.DialogInputAdjustBinding;
import org.dolphinemu.dolphinemu.databinding.DialogNfcFiguresManagerBinding;
import org.dolphinemu.dolphinemu.features.infinitybase.InfinityConfig;
import org.dolphinemu.dolphinemu.features.infinitybase.model.Figure;
import org.dolphinemu.dolphinemu.features.infinitybase.ui.FigureSlot;
import org.dolphinemu.dolphinemu.features.infinitybase.ui.FigureSlotAdapter;
import org.dolphinemu.dolphinemu.features.input.model.ControllerInterface;
import org.dolphinemu.dolphinemu.features.input.model.DolphinSensorEventListener;
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivity;
import org.dolphinemu.dolphinemu.features.skylanders.SkylanderConfig;
import org.dolphinemu.dolphinemu.features.skylanders.model.Skylander;
import org.dolphinemu.dolphinemu.features.skylanders.ui.SkylanderSlot;
import org.dolphinemu.dolphinemu.features.skylanders.ui.SkylanderSlotAdapter;
import org.dolphinemu.dolphinemu.fragments.EmulationFragment;
import org.dolphinemu.dolphinemu.fragments.MenuFragment;
import org.dolphinemu.dolphinemu.fragments.SaveLoadStateFragment;
import org.dolphinemu.dolphinemu.overlay.InputOverlay;
import org.dolphinemu.dolphinemu.overlay.InputOverlayPointer;
import org.dolphinemu.dolphinemu.ui.main.MainPresenter;
import org.dolphinemu.dolphinemu.ui.main.ThemeProvider;
import org.dolphinemu.dolphinemu.utils.AfterDirectoryInitializationRunner;
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper;
import org.dolphinemu.dolphinemu.utils.ThemeHelper;

import java.lang.annotation.Retention;
import java.util.ArrayList;
import java.util.List;

import static java.lang.annotation.RetentionPolicy.SOURCE;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.android.material.slider.Slider;

public final class EmulationActivity extends AppCompatActivity implements ThemeProvider
{
  private static final String BACKSTACK_NAME_MENU = "menu";
  private static final String BACKSTACK_NAME_SUBMENU = "submenu";
  public static final int REQUEST_CHANGE_DISC = 1;
  public static final int REQUEST_SKYLANDER_FILE = 2;
  public static final int REQUEST_CREATE_SKYLANDER = 3;
  public static final int REQUEST_INFINITY_FIGURE_FILE = 4;
  public static final int REQUEST_CREATE_INFINITY_FIGURE = 5;

  private EmulationFragment mEmulationFragment;

  private SharedPreferences mPreferences;

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
  public static final String EXTRA_SKYLANDER_SLOT = "SkylanderSlot";
  public static final String EXTRA_SKYLANDER_ID = "SkylanderId";
  public static final String EXTRA_SKYLANDER_VAR = "SkylanderVar";
  public static final String EXTRA_SKYLANDER_NAME = "SkylanderName";
  public static final String EXTRA_INFINITY_POSITION = "FigurePosition";
  public static final String EXTRA_INFINITY_LIST_POSITION = "FigureListPosition";
  public static final String EXTRA_INFINITY_NUM = "FigureNum";
  public static final String EXTRA_INFINITY_NAME = "FigureName";

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
                  MENU_ACTION_CHOOSE_DOUBLETAP, MENU_ACTION_PAUSE_EMULATION,
                  MENU_ACTION_UNPAUSE_EMULATION, MENU_ACTION_OVERLAY_CONTROLS, MENU_ACTION_SETTINGS,
                  MENU_ACTION_SKYLANDERS, MENU_ACTION_INFINITY_BASE})
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
  public static final int MENU_ACTION_RESET_OVERLAY = 26;
  public static final int MENU_SET_IR_RECENTER = 27;
  public static final int MENU_SET_IR_MODE = 28;
  public static final int MENU_ACTION_CHOOSE_DOUBLETAP = 30;
  public static final int MENU_ACTION_PAUSE_EMULATION = 32;
  public static final int MENU_ACTION_UNPAUSE_EMULATION = 33;
  public static final int MENU_ACTION_OVERLAY_CONTROLS = 34;
  public static final int MENU_ACTION_SETTINGS = 35;
  public static final int MENU_ACTION_SKYLANDERS = 36;
  public static final int MENU_ACTION_INFINITY_BASE = 37;

  private Skylander mSkylanderData = new Skylander(-1, -1, "Slot");
  private Figure mInfinityFigureData = new Figure(-1, "Position");

  private int mSkylanderSlot = -1;
  private int mInfinityPosition = -1;
  private int mInfinityListPosition = -1;

  private DialogNfcFiguresManagerBinding mSkylandersBinding;
  private DialogNfcFiguresManagerBinding mInfinityBinding;

  private static List<SkylanderSlot> sSkylanderSlots = new ArrayList<>();
  private static List<FigureSlot> sInfinityFigures = new ArrayList<>();

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
    buttonsActionsMap
            .append(R.id.menu_emulation_reset_overlay, EmulationActivity.MENU_ACTION_RESET_OVERLAY);
    buttonsActionsMap.append(R.id.menu_emulation_ir_recenter,
            EmulationActivity.MENU_SET_IR_RECENTER);
    buttonsActionsMap.append(R.id.menu_emulation_set_ir_mode,
            EmulationActivity.MENU_SET_IR_MODE);
    buttonsActionsMap.append(R.id.menu_emulation_choose_doubletap,
            EmulationActivity.MENU_ACTION_CHOOSE_DOUBLETAP);
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

    // Set these options now so that the SurfaceView the game renders into is the right size.
    enableFullscreenImmersive();

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

    if (sSkylanderSlots.isEmpty())
    {
      for (int i = 0; i < 8; i++)
      {
        sSkylanderSlots.add(new SkylanderSlot(getString(R.string.skylander_slot, i + 1), i));
      }
    }

    if (sInfinityFigures.isEmpty())
    {
      sInfinityFigures.add(new FigureSlot(getString(R.string.infinity_hexagon_label), 0));
      sInfinityFigures.add(new FigureSlot(getString(R.string.infinity_p1_label), 1));
      sInfinityFigures.add(new FigureSlot(getString(R.string.infinity_p1a1_label), 3));
      sInfinityFigures.add(new FigureSlot(getString(R.string.infinity_p1a2_label), 5));
      sInfinityFigures.add(new FigureSlot(getString(R.string.infinity_p2_label), 2));
      sInfinityFigures.add(new FigureSlot(getString(R.string.infinity_p2a1_label), 4));
      sInfinityFigures.add(new FigureSlot(getString(R.string.infinity_p2a2_label), 6));
    }
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
    outState.putInt(EXTRA_SKYLANDER_SLOT, mSkylanderSlot);
    outState.putInt(EXTRA_SKYLANDER_ID, mSkylanderData.getId());
    outState.putInt(EXTRA_SKYLANDER_VAR, mSkylanderData.getVariant());
    outState.putString(EXTRA_SKYLANDER_NAME, mSkylanderData.getName());
    outState.putInt(EXTRA_INFINITY_POSITION, mInfinityPosition);
    outState.putInt(EXTRA_INFINITY_LIST_POSITION, mInfinityListPosition);
    outState.putLong(EXTRA_INFINITY_NUM, mInfinityFigureData.getNumber());
    outState.putString(EXTRA_INFINITY_NAME, mInfinityFigureData.getName());
    super.onSaveInstanceState(outState);
  }

  protected void restoreState(Bundle savedInstanceState)
  {
    mPaths = savedInstanceState.getStringArray(EXTRA_SELECTED_GAMES);
    sUserPausedEmulation = savedInstanceState.getBoolean(EXTRA_USER_PAUSED_EMULATION);
    mMenuToastShown = savedInstanceState.getBoolean(EXTRA_MENU_TOAST_SHOWN);
    mSkylanderSlot = savedInstanceState.getInt(EXTRA_SKYLANDER_SLOT);
    mSkylanderData = new Skylander(savedInstanceState.getInt(EXTRA_SKYLANDER_ID),
            savedInstanceState.getInt(EXTRA_SKYLANDER_VAR),
            savedInstanceState.getString(EXTRA_SKYLANDER_NAME));
    mInfinityPosition = savedInstanceState.getInt(EXTRA_INFINITY_POSITION);
    mInfinityListPosition = savedInstanceState.getInt(EXTRA_INFINITY_LIST_POSITION);
    mInfinityFigureData = new Figure(savedInstanceState.getLong(EXTRA_INFINITY_NUM),
            savedInstanceState.getString(EXTRA_INFINITY_NAME));
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
              BooleanSetting.MAIN_EXPAND_TO_CUTOUT_AREA.getBoolean() ?
                      WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES :
                      WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER;

      getWindow().setAttributes(attributes);
    }

    updateOrientation();

    DolphinSensorEventListener.setDeviceRotation(
            getWindowManager().getDefaultDisplay().getRotation());
  }

  @Override
  protected void onPause()
  {
    super.onPause();
  }

  @Override
  protected void onStop()
  {
    super.onStop();
    mSettings.saveSettings(null);
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

    mEmulationFragment.refreshInputOverlay();
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
    // If the user picked a file, as opposed to just backing out.
    if (resultCode == RESULT_OK)
    {
      if (requestCode == REQUEST_CHANGE_DISC)
      {
        NativeLibrary.ChangeDisc(result.getData().toString());
      }
      else if (requestCode == REQUEST_SKYLANDER_FILE)
      {
        Pair<Integer, String> slot =
                SkylanderConfig.loadSkylander(sSkylanderSlots.get(mSkylanderSlot).getPortalSlot(),
                        result.getData().toString());
        clearSkylander(mSkylanderSlot);
        sSkylanderSlots.get(mSkylanderSlot).setPortalSlot(slot.first);
        sSkylanderSlots.get(mSkylanderSlot).setLabel(slot.second);
        mSkylandersBinding.figureManager.getAdapter().notifyItemChanged(mSkylanderSlot);
        mSkylanderSlot = -1;
        mSkylanderData = Skylander.BLANK_SKYLANDER;
      }
      else if (requestCode == REQUEST_CREATE_SKYLANDER)
      {
        if (!(mSkylanderData.getId() == -1) && !(mSkylanderData.getVariant() == -1))
        {
          Pair<Integer, String> slot = SkylanderConfig.createSkylander(mSkylanderData.getId(),
                  mSkylanderData.getVariant(),
                  result.getData().toString(), sSkylanderSlots.get(mSkylanderSlot).getPortalSlot());
          clearSkylander(mSkylanderSlot);
          sSkylanderSlots.get(mSkylanderSlot).setPortalSlot(slot.first);
          sSkylanderSlots.get(mSkylanderSlot).setLabel(slot.second);
          mSkylandersBinding.figureManager.getAdapter().notifyItemChanged(mSkylanderSlot);
          mSkylanderSlot = -1;
          mSkylanderData = Skylander.BLANK_SKYLANDER;
        }
      }
      else if (requestCode == REQUEST_INFINITY_FIGURE_FILE)
      {
        String label = InfinityConfig.loadFigure(mInfinityPosition, result.getData().toString());
        if (label != null && !label.equals("Unknown Figure"))
        {
          clearInfinityFigure(mInfinityListPosition);
          sInfinityFigures.get(mInfinityListPosition).setLabel(label);
          mInfinityBinding.figureManager.getAdapter().notifyItemChanged(mInfinityListPosition);
          mInfinityPosition = -1;
          mInfinityListPosition = -1;
          mInfinityFigureData = Figure.BLANK_FIGURE;
        }
        else
        {
          new MaterialAlertDialogBuilder(this)
                  .setTitle(R.string.incompatible_figure_selected)
                  .setMessage(R.string.select_compatible_figure)
                  .setPositiveButton(R.string.ok, null)
                  .show();
        }
      }
      else if (requestCode == REQUEST_CREATE_INFINITY_FIGURE)
      {
        if (!(mInfinityFigureData.getNumber() == -1))
        {
          String label =
                  InfinityConfig.createFigure(mInfinityFigureData.getNumber(),
                          result.getData().toString(),
                          mInfinityPosition);
          clearInfinityFigure(mInfinityListPosition);
          sInfinityFigures.get(mInfinityListPosition).setLabel(label);
          mInfinityBinding.figureManager.getAdapter().notifyItemChanged(mInfinityListPosition);
          mInfinityPosition = -1;
          mInfinityListPosition = -1;
          mInfinityFigureData = Figure.BLANK_FIGURE;
        }
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
    setRequestedOrientation(IntSetting.MAIN_EMULATION_ORIENTATION.getInt());
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

    // Populate the switch value for joystick center on touch
    menu.findItem(R.id.menu_emulation_joystick_rel_center)
            .setChecked(BooleanSetting.MAIN_JOYSTICK_REL_CENTER.getBoolean());
    if (wii)
    {
      menu.findItem(R.id.menu_emulation_ir_recenter)
              .setChecked(BooleanSetting.MAIN_IR_ALWAYS_RECENTER.getBoolean());
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
        // Need to pass a reference to the item to set the switch state, since it is not done automatically
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

      // Change the controller for the input overlay.
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

      case MENU_ACTION_CHOOSE_DOUBLETAP:
        chooseDoubleTapButton();
        break;

      case MENU_ACTION_SETTINGS:
        SettingsActivity.launch(this, MenuTag.SETTINGS);
        break;

      case MENU_ACTION_SKYLANDERS:
        showSkylanderPortalSettings();
        break;

      case MENU_ACTION_INFINITY_BASE:
        showInfinityBaseSettings();
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

  private void toggleRecenter(boolean state)
  {
    BooleanSetting.MAIN_IR_ALWAYS_RECENTER.setBoolean(mSettings, state);
    mEmulationFragment.refreshOverlayPointer();
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
    if (!mMenuVisible)
    {
      if (ControllerInterface.dispatchKeyEvent(event))
      {
        return true;
      }
    }

    return super.dispatchKeyEvent(event);
  }

  private void toggleControls()
  {
    MaterialAlertDialogBuilder builder = new MaterialAlertDialogBuilder(this)
            .setTitle(R.string.emulation_toggle_controls);

    int currentController = InputOverlay.getConfiguredControllerType();

    if (currentController == InputOverlay.OVERLAY_GAMECUBE)
    {
      boolean[] gcEnabledButtons = new boolean[11];
      String gcSettingBase = "MAIN_BUTTON_TOGGLE_GC_";

      for (int i = 0; i < gcEnabledButtons.length; i++)
      {
        gcEnabledButtons[i] = BooleanSetting.valueOf(gcSettingBase + i).getBoolean();
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
                BooleanSetting.valueOf(classicSettingBase + i).getBoolean();
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
        wiiEnabledButtons[i] = BooleanSetting.valueOf(wiiSettingBase + i).getBoolean();
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
    int currentValue = IntSetting.MAIN_DOUBLE_TAP_BUTTON.getInt();

    int buttonList = InputOverlay.getConfiguredControllerType() ==
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
    scaleSlider.setValue(IntSetting.MAIN_CONTROL_SCALE.getInt());
    scaleSlider.setStepSize(1);
    scaleSlider.addOnChangeListener(
            (slider, progress, fromUser) -> scaleValue.setText(((int) progress + 50) + "%"));
    scaleValue.setText(((int) scaleSlider.getValue() + 50) + "%");

    // alpha
    final Slider sliderOpacity = dialogBinding.inputOpacitySlider;
    final TextView valueOpacity = dialogBinding.inputOpacityValue;
    sliderOpacity.setValueTo(100);
    sliderOpacity.setValue(IntSetting.MAIN_CONTROL_OPACITY.getInt());
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

  private void addControllerIfNotNone(List<CharSequence> entries, List<Integer> values,
          IntSetting controller, int entry, int value)
  {
    if (controller.getInt() != 0)
    {
      entries.add(getString(entry));
      values.add(value);
    }
  }

  private void chooseController()
  {
    ArrayList<CharSequence> entries = new ArrayList<>();
    ArrayList<Integer> values = new ArrayList<>();

    entries.add(getString(R.string.none));
    values.add(-1);

    addControllerIfNotNone(entries, values, IntSetting.MAIN_SI_DEVICE_0, R.string.controller_0, 0);
    addControllerIfNotNone(entries, values, IntSetting.MAIN_SI_DEVICE_1, R.string.controller_1, 1);
    addControllerIfNotNone(entries, values, IntSetting.MAIN_SI_DEVICE_2, R.string.controller_2, 2);
    addControllerIfNotNone(entries, values, IntSetting.MAIN_SI_DEVICE_3, R.string.controller_3, 3);

    if (NativeLibrary.IsEmulatingWii())
    {
      addControllerIfNotNone(entries, values, IntSetting.WIIMOTE_1_SOURCE, R.string.wiimote_0, 4);
      addControllerIfNotNone(entries, values, IntSetting.WIIMOTE_2_SOURCE, R.string.wiimote_1, 5);
      addControllerIfNotNone(entries, values, IntSetting.WIIMOTE_3_SOURCE, R.string.wiimote_2, 6);
      addControllerIfNotNone(entries, values, IntSetting.WIIMOTE_4_SOURCE, R.string.wiimote_3, 7);
    }

    IntSetting controllerSetting = NativeLibrary.IsEmulatingWii() ?
            IntSetting.MAIN_OVERLAY_WII_CONTROLLER : IntSetting.MAIN_OVERLAY_GC_CONTROLLER;
    int currentValue = controllerSetting.getInt();

    int checkedItem = -1;
    for (int i = 0; i < values.size(); i++)
    {
      if (values.get(i) == currentValue)
      {
        checkedItem = i;
        break;
      }
    }

    final SharedPreferences.Editor editor = mPreferences.edit();
    new MaterialAlertDialogBuilder(this)
            .setTitle(R.string.emulation_choose_controller)
            .setSingleChoiceItems(entries.toArray(new CharSequence[]{}), checkedItem,
                    (dialog, indexSelected) ->
                            controllerSetting.setInt(mSettings, values.get(indexSelected)))
            .setPositiveButton(R.string.ok, (dialogInterface, i) ->
            {
              editor.apply();
              mEmulationFragment.refreshInputOverlay();
            })
            .setNeutralButton(R.string.emulation_more_controller_settings,
                    (dialogInterface, i) -> SettingsActivity.launch(this, MenuTag.SETTINGS))
            .show();
  }

  private void setIRMode()
  {
    new MaterialAlertDialogBuilder(this)
            .setTitle(R.string.emulation_ir_mode)
            .setSingleChoiceItems(R.array.irModeEntries,
                    IntSetting.MAIN_IR_MODE.getInt(),
                    (dialog, indexSelected) ->
                            IntSetting.MAIN_IR_MODE.setInt(mSettings, indexSelected))
            .setPositiveButton(R.string.ok, (dialogInterface, i) ->
                    mEmulationFragment.refreshOverlayPointer())
            .show();
  }

  private void showSkylanderPortalSettings()
  {
    mSkylandersBinding =
            DialogNfcFiguresManagerBinding.inflate(getLayoutInflater());
    mSkylandersBinding.figureManager.setLayoutManager(new LinearLayoutManager(this));

    mSkylandersBinding.figureManager.setAdapter(
            new SkylanderSlotAdapter(sSkylanderSlots, this));

    new MaterialAlertDialogBuilder(this)
            .setTitle(R.string.skylanders_manager)
            .setView(mSkylandersBinding.getRoot())
            .show();
  }

  private void showInfinityBaseSettings()
  {
    mInfinityBinding =
            DialogNfcFiguresManagerBinding.inflate(getLayoutInflater());
    mInfinityBinding.figureManager.setLayoutManager(new LinearLayoutManager(this));

    mInfinityBinding.figureManager.setAdapter(
            new FigureSlotAdapter(sInfinityFigures, this));

    new MaterialAlertDialogBuilder(this)
            .setTitle(R.string.infinity_manager)
            .setView(mInfinityBinding.getRoot())
            .show();
  }

  public void setSkylanderData(int id, int var, String name, int slot)
  {
    mSkylanderData = new Skylander(id, var, name);
    mSkylanderSlot = slot;
  }

  public void clearSkylander(int slot)
  {
    sSkylanderSlots.get(slot).setLabel(getString(R.string.skylander_slot, slot + 1));
    mSkylandersBinding.figureManager.getAdapter().notifyItemChanged(slot);
  }

  public void setInfinityFigureData(Long num, String name, int position, int listPosition)
  {
    mInfinityFigureData = new Figure(num, name);
    mInfinityPosition = position;
    mInfinityListPosition = listPosition;
  }

  public void clearInfinityFigure(int position)
  {
    switch (position)
    {
      case 0:
        sInfinityFigures.get(position).setLabel(getString(R.string.infinity_hexagon_label));
        break;
      case 1:
        sInfinityFigures.get(position).setLabel(getString(R.string.infinity_p1_label));
        break;
      case 2:
        sInfinityFigures.get(position).setLabel(getString(R.string.infinity_p1a1_label));
        break;
      case 3:
        sInfinityFigures.get(position).setLabel(getString(R.string.infinity_p1a2_label));
        break;
      case 4:
        sInfinityFigures.get(position).setLabel(getString(R.string.infinity_p2_label));
        break;
      case 5:
        sInfinityFigures.get(position).setLabel(getString(R.string.infinity_p2a1_label));
        break;
      case 6:
        sInfinityFigures.get(position).setLabel(getString(R.string.infinity_p2a2_label));
        break;
    }
    mInfinityBinding.figureManager.getAdapter().notifyItemChanged(position);
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
    if (!mMenuVisible)
    {
      if (ControllerInterface.dispatchGenericMotionEvent(event))
      {
        return true;
      }
    }

    return super.dispatchGenericMotionEvent(event);
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
