package org.dolphinemu.dolphinemu.fragments;

import android.content.pm.PackageManager;
import android.graphics.Rect;
import android.os.Bundle;
import android.util.SparseIntArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.fragment.app.Fragment;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public final class MenuFragment extends Fragment implements View.OnClickListener
{
  private TextView mTitleText;
  private View mPauseEmulation;
  private View mUnpauseEmulation;

  private static final String KEY_TITLE = "title";
  private static SparseIntArray buttonsActionsMap = new SparseIntArray();

  static
  {
    buttonsActionsMap
            .append(R.id.menu_pause_emulation, EmulationActivity.MENU_ACTION_PAUSE_EMULATION);
    buttonsActionsMap
            .append(R.id.menu_unpause_emulation, EmulationActivity.MENU_ACTION_UNPAUSE_EMULATION);
    buttonsActionsMap
            .append(R.id.menu_take_screenshot, EmulationActivity.MENU_ACTION_TAKE_SCREENSHOT);
    buttonsActionsMap.append(R.id.menu_quicksave, EmulationActivity.MENU_ACTION_QUICK_SAVE);
    buttonsActionsMap.append(R.id.menu_quickload, EmulationActivity.MENU_ACTION_QUICK_LOAD);
    buttonsActionsMap
            .append(R.id.menu_emulation_save_root, EmulationActivity.MENU_ACTION_SAVE_ROOT);
    buttonsActionsMap
            .append(R.id.menu_emulation_load_root, EmulationActivity.MENU_ACTION_LOAD_ROOT);
    buttonsActionsMap
            .append(R.id.menu_overlay_controls, EmulationActivity.MENU_ACTION_OVERLAY_CONTROLS);
    buttonsActionsMap
            .append(R.id.menu_refresh_wiimotes, EmulationActivity.MENU_ACTION_REFRESH_WIIMOTES);
    buttonsActionsMap
            .append(R.id.menu_screen_orientation, EmulationActivity.MENU_ACTION_SCREEN_ORIENTATION);
    buttonsActionsMap.append(R.id.menu_change_disc, EmulationActivity.MENU_ACTION_CHANGE_DISC);
    buttonsActionsMap.append(R.id.menu_exit, EmulationActivity.MENU_ACTION_EXIT);
  }

  public static MenuFragment newInstance(String title)
  {
    MenuFragment fragment = new MenuFragment();

    Bundle arguments = new Bundle();
    arguments.putSerializable(KEY_TITLE, title);
    fragment.setArguments(arguments);

    return fragment;
  }

  // This is primarily intended to account for any navigation bar at the bottom of the screen
  private int getBottomPaddingRequired()
  {
    Rect visibleFrame = new Rect();
    requireActivity().getWindow().getDecorView().getWindowVisibleDisplayFrame(visibleFrame);
    return visibleFrame.bottom - visibleFrame.top - getResources().getDisplayMetrics().heightPixels;
  }

  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
  {
    View rootView = inflater.inflate(R.layout.fragment_ingame_menu, container, false);

    LinearLayout options = rootView.findViewById(R.id.layout_options);

    mPauseEmulation = options.findViewById(R.id.menu_pause_emulation);
    mUnpauseEmulation = options.findViewById(R.id.menu_unpause_emulation);

    updatePauseUnpauseVisibility();

    Settings settings = ((EmulationActivity) getActivity()).getSettings();
    if (BooleanSetting.MAIN_ENABLE_SAVESTATES.getBoolean(settings))
    {
      options.findViewById(R.id.menu_quicksave).setVisibility(View.VISIBLE);
      options.findViewById(R.id.menu_quickload).setVisibility(View.VISIBLE);
      options.findViewById(R.id.menu_emulation_save_root).setVisibility(View.VISIBLE);
      options.findViewById(R.id.menu_emulation_load_root).setVisibility(View.VISIBLE);
    }

    PackageManager packageManager = requireActivity().getPackageManager();

    if (!packageManager.hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN))
    {
      options.findViewById(R.id.menu_overlay_controls).setVisibility(View.GONE);
    }

    if (EmulationActivity.isGameCubeGame())
    {
      options.findViewById(R.id.menu_refresh_wiimotes).setVisibility(View.GONE);
    }

    // Old devices which support both portrait and landscape may report support for neither,
    // so we only hide the orientation button if the device only supports one orientation
    if (packageManager.hasSystemFeature(PackageManager.FEATURE_SCREEN_PORTRAIT) !=
            packageManager.hasSystemFeature(PackageManager.FEATURE_SCREEN_LANDSCAPE))
    {
      options.findViewById(R.id.menu_screen_orientation).setVisibility(View.GONE);
    }

    int bottomPaddingRequired = getBottomPaddingRequired();

    // Provide a safe zone between the navigation bar and Exit Emulation to avoid accidental touches
    float density = getResources().getDisplayMetrics().density;
    if (bottomPaddingRequired >= 32 * density)
    {
      bottomPaddingRequired += 32 * density;
    }

    if (bottomPaddingRequired > rootView.getPaddingBottom())
    {
      rootView.setPadding(rootView.getPaddingLeft(), rootView.getPaddingTop(),
              rootView.getPaddingRight(), bottomPaddingRequired);
    }

    for (int childIndex = 0; childIndex < options.getChildCount(); childIndex++)
    {
      Button button = (Button) options.getChildAt(childIndex);

      button.setOnClickListener(this);
    }

    rootView.findViewById(R.id.menu_exit).setOnClickListener(this);

    mTitleText = rootView.findViewById(R.id.text_game_title);
    String title = getArguments().getString(KEY_TITLE);
    if (title != null)
    {
      mTitleText.setText(title);
    }

    return rootView;
  }

  private void updatePauseUnpauseVisibility()
  {
    boolean paused = EmulationActivity.getHasUserPausedEmulation();

    mUnpauseEmulation.setVisibility(paused ? View.VISIBLE : View.GONE);
    mPauseEmulation.setVisibility(paused ? View.GONE : View.VISIBLE);
  }

  @SuppressWarnings("WrongConstant")
  @Override
  public void onClick(View button)
  {
    int action = buttonsActionsMap.get(button.getId());
    EmulationActivity activity = (EmulationActivity) requireActivity();

    if (action == EmulationActivity.MENU_ACTION_OVERLAY_CONTROLS)
    {
      // We could use the button parameter as the anchor here, but this often results in a tiny menu
      // (because the button often is in the middle of the screen), so let's use mTitleText instead
      activity.showOverlayControlsMenu(mTitleText);
    }
    else if (action >= 0)
    {
      activity.handleMenuAction(action);
    }

    if (action == EmulationActivity.MENU_ACTION_PAUSE_EMULATION ||
            action == EmulationActivity.MENU_ACTION_UNPAUSE_EMULATION)
    {
      updatePauseUnpauseVisibility();
    }
  }
}
