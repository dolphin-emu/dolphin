package org.dolphinemu.dolphinemu.features.settings.ui;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.Setting;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.ui.DividerItemDecoration;

import java.util.ArrayList;

public final class SettingsFragment extends Fragment implements SettingsFragmentView
{
  private static final String ARGUMENT_MENU_TAG = "menu_tag";
  private static final String ARGUMENT_GAME_ID = "game_id";

  private SettingsFragmentPresenter mPresenter = new SettingsFragmentPresenter(this);
  private SettingsActivityView mActivity;

  private SettingsAdapter mAdapter;

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
    mPresenter.onAttach();
  }

  /**
   * This version of onAttach is needed for versions below Marshmallow.
   *
   * @param activity
   */
  @Override
  public void onAttach(Activity activity)
  {
    super.onAttach(activity);

    mActivity = (SettingsActivityView) activity;
    mPresenter.onAttach();
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
    LinearLayoutManager manager = new LinearLayoutManager(getActivity());

    RecyclerView recyclerView = (RecyclerView) view.findViewById(R.id.list_settings);

    recyclerView.setAdapter(mAdapter);
    recyclerView.setLayoutManager(manager);
    recyclerView.addItemDecoration(new DividerItemDecoration(getActivity(), null));

    SettingsActivityView activity = (SettingsActivityView) getActivity();
    mPresenter.onViewCreated(activity.getSettings());
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
  public void passSettingsToActivity(
          org.dolphinemu.dolphinemu.features.settings.model.Settings settings)
  {
    if (mActivity != null)
    {
      mActivity.setSettings(settings);
    }
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
  public void onSettingChanged()
  {
    mActivity.onSettingChanged();
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
