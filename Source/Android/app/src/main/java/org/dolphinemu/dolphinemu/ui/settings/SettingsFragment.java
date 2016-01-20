package org.dolphinemu.dolphinemu.ui.settings;

import android.content.Context;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.dolphinemu.dolphinemu.BuildConfig;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.settings.SettingSection;
import org.dolphinemu.dolphinemu.model.settings.view.SettingsItem;
import org.dolphinemu.dolphinemu.ui.DividerItemDecoration;

import java.util.ArrayList;
import java.util.HashMap;

public final class SettingsFragment extends Fragment implements SettingsFragmentView
{
	private SettingsFragmentPresenter mPresenter = new SettingsFragmentPresenter(this);
	private SettingsActivityView mView;

	private SettingsAdapter mAdapter;

	@Override
	public void onAttach(Context context)
	{
		super.onAttach(context);

		mView = (SettingsActivityView) context;
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
		HashMap<String, SettingSection> settings = activity.getSettings();

		mPresenter.onViewCreated(settings);
	}

	@Override
	public void onDetach()
	{
		super.onDetach();
		mView = null;

		if (mAdapter != null)
		{
			mAdapter.closeDialog();
		}
	}

	@Override
	public void onSettingsFileLoaded(HashMap<String, SettingSection> settings)
	{
		mPresenter.setSettings(settings);
	}

	@Override
	public void passOptionsToActivity(HashMap<String, SettingSection> settings)
	{
		if (mView != null)
		{
			mView.setSettings(settings);
		}
	}

	@Override
	public void showSettingsList(ArrayList<SettingsItem> settingsList)
	{
		mAdapter.setSettings(settingsList);
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
