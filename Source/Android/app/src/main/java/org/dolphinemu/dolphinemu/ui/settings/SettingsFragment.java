package org.dolphinemu.dolphinemu.ui.settings;

import android.app.Activity;
import android.app.Fragment;
import android.content.Context;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.dolphinemu.dolphinemu.BuildConfig;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.settings.Setting;
import org.dolphinemu.dolphinemu.model.settings.SettingSection;
import org.dolphinemu.dolphinemu.model.settings.view.SettingsItem;
import org.dolphinemu.dolphinemu.ui.DividerItemDecoration;
import org.dolphinemu.dolphinemu.utils.SettingsFile;

import java.util.ArrayList;
import java.util.HashMap;

public final class SettingsFragment extends Fragment implements SettingsFragmentView
{
	private SettingsFragmentPresenter mPresenter = new SettingsFragmentPresenter(this);
	private SettingsActivityView mActivity;

	private SettingsAdapter mAdapter;

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
		String menuTag = getArguments().getString(ARGUMENT_MENU_TAG);

		mAdapter = new SettingsAdapter(this, getActivity());

		mPresenter.onCreate(menuTag);
	}

	@Nullable
	@Override
	public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState)
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

		ArrayList<HashMap<String, SettingSection>> settings = new ArrayList<>();
		settings.add(SettingsFile.SETTINGS_DOLPHIN, activity.getSettings(SettingsFile.SETTINGS_DOLPHIN));
		settings.add(SettingsFile.SETTINGS_GFX, activity.getSettings(SettingsFile.SETTINGS_GFX));
		settings.add(SettingsFile.SETTINGS_WIIMOTE, activity.getSettings(SettingsFile.SETTINGS_WIIMOTE));

		mPresenter.onViewCreated(settings);
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
	public void onSettingsFileLoaded(ArrayList<HashMap<String, SettingSection>> settings)
	{
		mPresenter.setSettings(settings);
	}

	@Override
	public void passSettingsToActivity(ArrayList<HashMap<String, SettingSection>> settings)
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
	public void loadSubMenu(String menuKey)
	{
		mActivity.showSettingsFragment(menuKey, true);
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
	public void onGcPadSettingChanged(String key, int value)
	{
		mActivity.onGcPadSettingChanged(key, value);
	}

	@Override
	public void onWiimoteSettingChanged(String section, int value)
	{
		mActivity.onWiimoteSettingChanged(section, value);
	}

	@Override
	public void onExtensionSettingChanged(String key, int value)
	{
		mActivity.onExtensionSettingChanged(key, value);
	}

	public static final String FRAGMENT_TAG = BuildConfig.APPLICATION_ID + ".fragment.settings";

	public static final String ARGUMENT_MENU_TAG = FRAGMENT_TAG + ".menu_tag";

	public static Fragment newInstance(String menuTag)
	{
		SettingsFragment fragment = new SettingsFragment();

		Bundle arguments = new Bundle();
		arguments.putString(ARGUMENT_MENU_TAG, menuTag);

		fragment.setArguments(arguments);
		return fragment;
	}
}
