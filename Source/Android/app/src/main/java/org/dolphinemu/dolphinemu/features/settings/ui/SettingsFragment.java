// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui;

import android.content.Context;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.databinding.FragmentSettingsBinding;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

public final class SettingsFragment extends Fragment implements SettingsFragmentView
{
  private static final String ARGUMENT_MENU_TAG = "menu_tag";
  private static final String ARGUMENT_GAME_ID = "game_id";

  private SettingsFragmentPresenter mPresenter;
  private SettingsActivityView mActivity;

  private SettingsAdapter mAdapter;

  private int mOldControllerSettingsWarningHeight = 0;

  private static final Map<MenuTag, Integer> titles = new HashMap<>();

  static
  {
    titles.put(MenuTag.SETTINGS, R.string.settings);
    titles.put(MenuTag.CONFIG, R.string.config);
    titles.put(MenuTag.CONFIG_GENERAL, R.string.general_submenu);
    titles.put(MenuTag.CONFIG_INTERFACE, R.string.interface_submenu);
    titles.put(MenuTag.CONFIG_AUDIO, R.string.audio_submenu);
    titles.put(MenuTag.CONFIG_PATHS, R.string.paths_submenu);
    titles.put(MenuTag.CONFIG_GAME_CUBE, R.string.gamecube_submenu);
    titles.put(MenuTag.CONFIG_SERIALPORT1, R.string.serialport1_submenu);
    titles.put(MenuTag.CONFIG_WII, R.string.wii_submenu);
    titles.put(MenuTag.CONFIG_ADVANCED, R.string.advanced_submenu);
    titles.put(MenuTag.DEBUG, R.string.debug_submenu);
    titles.put(MenuTag.GRAPHICS, R.string.graphics_settings);
    titles.put(MenuTag.ENHANCEMENTS, R.string.enhancements_submenu);
    titles.put(MenuTag.STEREOSCOPY, R.string.stereoscopy_submenu);
    titles.put(MenuTag.HACKS, R.string.hacks_submenu);
    titles.put(MenuTag.STATISTICS, R.string.statistics_submenu);
    titles.put(MenuTag.ADVANCED_GRAPHICS, R.string.advanced_graphics_submenu);
    titles.put(MenuTag.CONFIG_LOG, R.string.log_submenu);
    titles.put(MenuTag.GCPAD_TYPE, R.string.gcpad_settings);
    titles.put(MenuTag.WIIMOTE, R.string.wiimote_settings);
    titles.put(MenuTag.WIIMOTE_EXTENSION, R.string.wiimote_extensions);
    titles.put(MenuTag.GCPAD_1, R.string.controller_0);
    titles.put(MenuTag.GCPAD_2, R.string.controller_1);
    titles.put(MenuTag.GCPAD_3, R.string.controller_2);
    titles.put(MenuTag.GCPAD_4, R.string.controller_3);
    titles.put(MenuTag.WIIMOTE_1, R.string.wiimote_0);
    titles.put(MenuTag.WIIMOTE_2, R.string.wiimote_1);
    titles.put(MenuTag.WIIMOTE_3, R.string.wiimote_2);
    titles.put(MenuTag.WIIMOTE_4, R.string.wiimote_3);
    titles.put(MenuTag.WIIMOTE_EXTENSION_1, R.string.wiimote_extension_0);
    titles.put(MenuTag.WIIMOTE_EXTENSION_2, R.string.wiimote_extension_1);
    titles.put(MenuTag.WIIMOTE_EXTENSION_3, R.string.wiimote_extension_2);
    titles.put(MenuTag.WIIMOTE_EXTENSION_4, R.string.wiimote_extension_3);
    titles.put(MenuTag.WIIMOTE_GENERAL_1, R.string.wiimote_general);
    titles.put(MenuTag.WIIMOTE_GENERAL_2, R.string.wiimote_general);
    titles.put(MenuTag.WIIMOTE_GENERAL_3, R.string.wiimote_general);
    titles.put(MenuTag.WIIMOTE_GENERAL_4, R.string.wiimote_general);
    titles.put(MenuTag.WIIMOTE_MOTION_SIMULATION_1, R.string.wiimote_motion_simulation);
    titles.put(MenuTag.WIIMOTE_MOTION_SIMULATION_2, R.string.wiimote_motion_simulation);
    titles.put(MenuTag.WIIMOTE_MOTION_SIMULATION_3, R.string.wiimote_motion_simulation);
    titles.put(MenuTag.WIIMOTE_MOTION_SIMULATION_4, R.string.wiimote_motion_simulation);
    titles.put(MenuTag.WIIMOTE_MOTION_INPUT_1, R.string.wiimote_motion_input);
    titles.put(MenuTag.WIIMOTE_MOTION_INPUT_2, R.string.wiimote_motion_input);
    titles.put(MenuTag.WIIMOTE_MOTION_INPUT_3, R.string.wiimote_motion_input);
    titles.put(MenuTag.WIIMOTE_MOTION_INPUT_4, R.string.wiimote_motion_input);
  }

  private FragmentSettingsBinding mBinding;

  public static Fragment newInstance(MenuTag menuTag, String gameId, Bundle extras)
  {
    SettingsFragment fragment = new SettingsFragment();

    Bundle arguments = new Bundle();
    if (extras != null)
    {
      arguments.putAll(extras);
    }

    arguments.putSerializable(ARGUMENT_MENU_TAG, menuTag);
    arguments.putString(ARGUMENT_GAME_ID, gameId);

    fragment.setArguments(arguments);
    return fragment;
  }

  @Override
  public void onAttach(@NonNull Context context)
  {
    super.onAttach(context);

    mActivity = (SettingsActivityView) context;
  }

  @Override
  public void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);

    Bundle args = getArguments();
    MenuTag menuTag = (MenuTag) args.getSerializable(ARGUMENT_MENU_TAG);
    String gameId = getArguments().getString(ARGUMENT_GAME_ID);

    mPresenter = new SettingsFragmentPresenter(this, getContext());
    mAdapter = new SettingsAdapter(this, getContext());

    mPresenter.onCreate(menuTag, gameId, args);
  }

  @NonNull
  @Override
  public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
          @Nullable Bundle savedInstanceState)
  {
    mBinding = FragmentSettingsBinding.inflate(inflater, container, false);
    return mBinding.getRoot();
  }

  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    Bundle args = getArguments();
    MenuTag menuTag = (MenuTag) args.getSerializable(ARGUMENT_MENU_TAG);

    if (titles.containsKey(menuTag))
    {
      mActivity.setToolbarTitle(getString(titles.get(menuTag)));
    }

    LinearLayoutManager manager = new LinearLayoutManager(getActivity());

    RecyclerView recyclerView = mBinding.listSettings;

    recyclerView.setAdapter(mAdapter);
    recyclerView.setLayoutManager(manager);

    SettingsDividerItemDecoration divider = new SettingsDividerItemDecoration(requireActivity());
    recyclerView.addItemDecoration(divider);

    setInsets();

    SettingsActivityView activity = (SettingsActivityView) getActivity();
    mPresenter.onViewCreated(menuTag, activity.getSettings());
  }

  @Override
  public void onDestroyView()
  {
    super.onDestroyView();
    mBinding = null;
  }

  @Override
  public void onDetach()
  {
    super.onDetach();
    mActivity = null;

    if (mAdapter != null)
    {
      mAdapter.closeDialog();
    }
  }

  @Override
  public void onSettingsFileLoaded(
          org.dolphinemu.dolphinemu.features.settings.model.Settings settings)
  {
    mPresenter.setSettings(settings);
  }

  @Override
  public void showSettingsList(ArrayList<SettingsItem> settingsList)
  {
    mAdapter.setSettings(settingsList);
  }

  @Override
  public void loadDefaultSettings()
  {
    mPresenter.loadDefaultSettings();
  }

  @Override
  public SettingsAdapter getAdapter()
  {
    return mAdapter;
  }

  @Override
  public void loadSubMenu(MenuTag menuKey)
  {
    mActivity.showSettingsFragment(menuKey, null, true, getArguments().getString(ARGUMENT_GAME_ID));
  }

  @Override
  public void showDialogFragment(DialogFragment fragment)
  {
    mActivity.showDialogFragment(fragment);
  }

  @Override
  public void showToastMessage(String message)
  {
    mActivity.showToastMessage(message);
  }

  @Override
  public Settings getSettings()
  {
    return mPresenter.getSettings();
  }

  @Override
  public void onSettingChanged()
  {
    mActivity.onSettingChanged();
  }

  @Override
  public void onControllerSettingsChanged()
  {
    mAdapter.notifyAllSettingsChanged();
    mPresenter.updateOldControllerSettingsWarningVisibility();
  }

  @Override
  public void onMenuTagAction(@NonNull MenuTag menuTag, int value)
  {
    mActivity.onMenuTagAction(menuTag, value);
  }

  public boolean hasMenuTagActionForValue(@NonNull MenuTag menuTag, int value)
  {
    return mActivity.hasMenuTagActionForValue(menuTag, value);
  }

  @Override
  public void setMappingAllDevices(boolean allDevices)
  {
    mActivity.setMappingAllDevices(allDevices);
  }

  @Override
  public boolean isMappingAllDevices()
  {
    return mActivity.isMappingAllDevices();
  }

  @Override
  public void setOldControllerSettingsWarningVisibility(boolean visible)
  {
    mOldControllerSettingsWarningHeight =
            mActivity.setOldControllerSettingsWarningVisibility(visible);

    // Trigger the insets listener we've registered
    mBinding.listSettings.requestApplyInsets();
  }

  private void setInsets()
  {
    ViewCompat.setOnApplyWindowInsetsListener(mBinding.listSettings, (v, windowInsets) ->
    {
      Insets insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars());
      int listSpacing = getResources().getDimensionPixelSize(R.dimen.spacing_list);
      v.setPadding(0, 0, 0, insets.bottom + listSpacing + mOldControllerSettingsWarningHeight);
      return windowInsets;
    });
  }
}
