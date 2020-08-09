package org.dolphinemu.dolphinemu.features.settings.ui;

import android.content.Context;
import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.Setting;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.ui.DividerItemDecoration;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

public final class SettingsFragment extends Fragment implements SettingsFragmentView
{
  private static final String ARGUMENT_MENU_TAG = "menu_tag";
  private static final String ARGUMENT_GAME_ID = "game_id";

  private SettingsFragmentPresenter mPresenter = new SettingsFragmentPresenter(this);
  private SettingsActivityView mActivity;

  private SettingsAdapter mAdapter;

  private static final Map<MenuTag, Integer> titles = new HashMap<>();

  static
  {
    titles.put(MenuTag.CONFIG, R.string.preferences_settings);
    titles.put(MenuTag.CONFIG_GENERAL, R.string.general_submenu);
    titles.put(MenuTag.CONFIG_INTERFACE, R.string.interface_submenu);
    titles.put(MenuTag.CONFIG_AUDIO, R.string.audio_submenu);
    titles.put(MenuTag.CONFIG_PATHS, R.string.paths_submenu);
    titles.put(MenuTag.CONFIG_GAME_CUBE, R.string.gamecube_submenu);
    titles.put(MenuTag.CONFIG_WII, R.string.wii_submenu);
    titles.put(MenuTag.CONFIG_ADVANCED, R.string.advanced_submenu);
    titles.put(MenuTag.WIIMOTE, R.string.grid_menu_wiimote_settings);
    titles.put(MenuTag.WIIMOTE_EXTENSION, R.string.wiimote_extensions);
    titles.put(MenuTag.GCPAD_TYPE, R.string.grid_menu_gcpad_settings);
    titles.put(MenuTag.GRAPHICS, R.string.grid_menu_graphics_settings);
    titles.put(MenuTag.HACKS, R.string.hacks_submenu);
    titles.put(MenuTag.CONFIG_LOG, R.string.log_submenu);
    titles.put(MenuTag.DEBUG, R.string.debug_submenu);
    titles.put(MenuTag.ENHANCEMENTS, R.string.enhancements_submenu);
    titles.put(MenuTag.STEREOSCOPY, R.string.stereoscopy_submenu);
    titles.put(MenuTag.GCPAD_1, R.string.controller_0);
    titles.put(MenuTag.GCPAD_2, R.string.controller_1);
    titles.put(MenuTag.GCPAD_3, R.string.controller_2);
    titles.put(MenuTag.GCPAD_4, R.string.controller_3);
    titles.put(MenuTag.WIIMOTE_1, R.string.wiimote_4);
    titles.put(MenuTag.WIIMOTE_2, R.string.wiimote_5);
    titles.put(MenuTag.WIIMOTE_3, R.string.wiimote_6);
    titles.put(MenuTag.WIIMOTE_4, R.string.wiimote_7);
    titles.put(MenuTag.WIIMOTE_EXTENSION_1, R.string.wiimote_extension_4);
    titles.put(MenuTag.WIIMOTE_EXTENSION_2, R.string.wiimote_extension_5);
    titles.put(MenuTag.WIIMOTE_EXTENSION_3, R.string.wiimote_extension_6);
    titles.put(MenuTag.WIIMOTE_EXTENSION_4, R.string.wiimote_extension_7);
  }

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
  public void onAttach(Context context)
  {
    super.onAttach(context);

    mActivity = (SettingsActivityView) context;
  }

  @Override
  public void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);

    setRetainInstance(true);
    Bundle args = getArguments();
    MenuTag menuTag = (MenuTag) args.getSerializable(ARGUMENT_MENU_TAG);
    String gameId = getArguments().getString(ARGUMENT_GAME_ID);

    mAdapter = new SettingsAdapter(this, getActivity());

    mPresenter.onCreate(menuTag, gameId, args);
  }

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
          @Nullable Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.fragment_settings, container, false);
  }

  @Override
  public void onViewCreated(View view, @Nullable Bundle savedInstanceState)
  {
    Bundle args = getArguments();
    MenuTag menuTag = (MenuTag) args.getSerializable(ARGUMENT_MENU_TAG);

    if (titles.containsKey(menuTag))
    {
      getActivity().setTitle(titles.get(menuTag));
    }

    LinearLayoutManager manager = new LinearLayoutManager(getActivity());

    RecyclerView recyclerView = view.findViewById(R.id.list_settings);

    recyclerView.setAdapter(mAdapter);
    recyclerView.setLayoutManager(manager);
    recyclerView.addItemDecoration(new DividerItemDecoration(requireActivity(), null));

    SettingsActivityView activity = (SettingsActivityView) getActivity();
    mPresenter.onViewCreated(menuTag, activity.getSettings());
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
  public void showToastMessage(String message)
  {
    mActivity.showToastMessage(message);
  }

  @Override
  public void putSetting(Setting setting)
  {
    mPresenter.putSetting(setting);
  }

  @Override
  public void onSettingChanged(String key)
  {
    mActivity.onSettingChanged(key);
  }

  @Override
  public void onGcPadSettingChanged(MenuTag menuTag, int value)
  {
    mActivity.onGcPadSettingChanged(menuTag, value);
  }

  @Override
  public void onWiimoteSettingChanged(MenuTag menuTag, int value)
  {
    mActivity.onWiimoteSettingChanged(menuTag, value);
  }

  @Override
  public void onExtensionSettingChanged(MenuTag menuTag, int value)
  {
    mActivity.onExtensionSettingChanged(menuTag, value);
  }
}
