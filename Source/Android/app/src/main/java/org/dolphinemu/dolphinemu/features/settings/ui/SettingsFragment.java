package org.dolphinemu.dolphinemu.features.settings.ui;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import com.nononsenseapps.filepicker.DividerItemDecoration;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;

import java.util.ArrayList;

public final class SettingsFragment extends Fragment implements SettingsFragmentView
{
  private static final String ARGUMENT_MENU_TAG = "menu_tag";
  private static final String ARGUMENT_GAME_ID = "game_id";

  private SettingsFragmentPresenter mPresenter;
  private ArrayList<SettingsItem> mSettingsList;
  private SettingsActivity mActivity;
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

    mActivity = (SettingsActivity) context;
    if(mPresenter == null)
      mPresenter = new SettingsFragmentPresenter(mActivity);
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

    mAdapter = new SettingsAdapter(mActivity);
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
    //LinearLayoutManager manager = new LinearLayoutManager(mActivity);
    Drawable lineDivider = mActivity.getDrawable(R.drawable.line_divider);
    RecyclerView recyclerView = view.findViewById(R.id.list_settings);

    GridLayoutManager mgr = new GridLayoutManager(mActivity, 2);
    mgr.setSpanSizeLookup(new GridLayoutManager.SpanSizeLookup()
    {
      @Override public int getSpanSize(int position)
      {
        int viewType = mAdapter.getItemViewType(position);
        if (SettingsItem.TYPE_INPUT_BINDING == viewType &&
          Settings.SECTION_BINDINGS.equals(mAdapter.getSettingSection(position)))
          return 1;
        return 2;
      }
    });

    recyclerView.setAdapter(mAdapter);
    recyclerView.setLayoutManager(mgr);
    recyclerView.addItemDecoration(new DividerItemDecoration(lineDivider));

    showSettingsList(mActivity.getSettings());
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

  public void showSettingsList(Settings settings)
  {
    if(mSettingsList == null && settings != null)
    {
      mSettingsList = mPresenter.loadSettingsList(settings);
    }

    if(mSettingsList != null)
    {
      mAdapter.setSettings(mSettingsList);
    }
  }

  public void closeDialog()
  {
    mAdapter.closeDialog();
  }
}
