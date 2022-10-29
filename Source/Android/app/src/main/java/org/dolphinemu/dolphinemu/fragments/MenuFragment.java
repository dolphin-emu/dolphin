// SPDX-License-Identifier: GPL-2.0-or-later

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

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.databinding.FragmentIngameMenuBinding;
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting;

public final class MenuFragment extends Fragment implements View.OnClickListener
{
  private static final String KEY_TITLE = "title";
  private static final String KEY_WII = "wii";
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
    buttonsActionsMap.append(R.id.menu_change_disc, EmulationActivity.MENU_ACTION_CHANGE_DISC);
    buttonsActionsMap.append(R.id.menu_exit, EmulationActivity.MENU_ACTION_EXIT);
    buttonsActionsMap.append(R.id.menu_settings, EmulationActivity.MENU_ACTION_SETTINGS);
  }

  private FragmentIngameMenuBinding mBinding;

  public static MenuFragment newInstance()
  {
    MenuFragment fragment = new MenuFragment();

    Bundle arguments = new Bundle();
    if (NativeLibrary.IsGameMetadataValid())
    {
      arguments.putString(KEY_TITLE, NativeLibrary.GetCurrentTitleDescription());
      arguments.putBoolean(KEY_WII, NativeLibrary.IsEmulatingWii());
    }
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

  @NonNull
  @Override
  public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container,
          Bundle savedInstanceState)
  {
    mBinding = FragmentIngameMenuBinding.inflate(inflater, container, false);
    return mBinding.getRoot();
  }

  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    updatePauseUnpauseVisibility();

    if (!requireActivity().getPackageManager().hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN))
    {
      mBinding.menuOverlayControls.setVisibility(View.GONE);
    }

    if (!getArguments().getBoolean(KEY_WII, true))
    {
      mBinding.menuRefreshWiimotes.setVisibility(View.GONE);
    }

    int bottomPaddingRequired = getBottomPaddingRequired();

    // Provide a safe zone between the navigation bar and Exit Emulation to avoid accidental touches
    float density = getResources().getDisplayMetrics().density;
    if (bottomPaddingRequired >= 32 * density)
    {
      bottomPaddingRequired += 32 * density;
    }

    if (bottomPaddingRequired > view.getPaddingBottom())
    {
      view.setPadding(view.getPaddingLeft(), view.getPaddingTop(),
              view.getPaddingRight(), bottomPaddingRequired);
    }

    LinearLayout options = mBinding.layoutOptions;
    for (int childIndex = 0; childIndex < options.getChildCount(); childIndex++)
    {
      Button button = (Button) options.getChildAt(childIndex);

      button.setOnClickListener(this);
    }

    mBinding.menuExit.setOnClickListener(this);

    String title = getArguments().getString(KEY_TITLE, null);
    if (title != null)
    {
      mBinding.textGameTitle.setText(title);
    }

    if (getResources().getConfiguration().getLayoutDirection() == View.LAYOUT_DIRECTION_LTR)
    {
      view.post(() -> NativeLibrary.SetObscuredPixelsLeft(view.getWidth()));
    }
  }

  @Override
  public void onResume()
  {
    super.onResume();

    boolean savestatesEnabled = BooleanSetting.MAIN_ENABLE_SAVESTATES.getBooleanGlobal();
    int savestateVisibility = savestatesEnabled ? View.VISIBLE : View.GONE;
    mBinding.menuQuicksave.setVisibility(savestateVisibility);
    mBinding.menuQuickload.setVisibility(savestateVisibility);
    mBinding.menuEmulationSaveRoot.setVisibility(savestateVisibility);
    mBinding.menuEmulationLoadRoot.setVisibility(savestateVisibility);
  }

  @Override
  public void onDestroyView()
  {
    super.onDestroyView();

    NativeLibrary.SetObscuredPixelsLeft(0);
    mBinding = null;
  }

  private void updatePauseUnpauseVisibility()
  {
    boolean paused = EmulationActivity.getHasUserPausedEmulation();

    mBinding.menuUnpauseEmulation.setVisibility(paused ? View.VISIBLE : View.GONE);
    mBinding.menuPauseEmulation.setVisibility(paused ? View.GONE : View.VISIBLE);
  }

  @Override
  public void onClick(View button)
  {
    int action = buttonsActionsMap.get(button.getId());
    EmulationActivity activity = (EmulationActivity) requireActivity();

    if (action == EmulationActivity.MENU_ACTION_OVERLAY_CONTROLS)
    {
      // We could use the button parameter as the anchor here, but this often results in a tiny menu
      // (because the button often is in the middle of the screen), so let's use mTitleText instead
      activity.showOverlayControlsMenu(mBinding.textGameTitle);
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
