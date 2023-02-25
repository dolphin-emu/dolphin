// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import org.dolphinemu.dolphinemu.databinding.FragmentCheatWarningBinding;
import org.dolphinemu.dolphinemu.features.settings.model.AbstractBooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivity;

public abstract class SettingDisabledWarningFragment extends Fragment
        implements View.OnClickListener
{
  private View mView;

  private final AbstractBooleanSetting mSetting;
  private final MenuTag mSettingShortcut;
  private final int mText;

  private FragmentCheatWarningBinding mBinding;

  public SettingDisabledWarningFragment(AbstractBooleanSetting setting, MenuTag settingShortcut,
          int text)
  {
    mSetting = setting;
    mSettingShortcut = settingShortcut;
    mText = text;
  }

  @Nullable
  @Override
  public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
          @Nullable Bundle savedInstanceState)
  {
    mBinding = FragmentCheatWarningBinding.inflate(inflater, container, false);
    return mBinding.getRoot();
  }

  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    mView = view;

    mBinding.textWarning.setText(mText);
    mBinding.buttonSettings.setOnClickListener(this);

    CheatsActivity activity = (CheatsActivity) requireActivity();
    CheatsActivity.setOnFocusChangeListenerRecursively(view,
            (v, hasFocus) -> activity.onListViewFocusChange(hasFocus));
  }

  @Override
  public void onResume()
  {
    super.onResume();

    CheatsActivity activity = (CheatsActivity) requireActivity();
    try (Settings settings = activity.loadGameSpecificSettings())
    {
      boolean cheatsEnabled = mSetting.getBoolean(settings);
      mView.setVisibility(cheatsEnabled ? View.GONE : View.VISIBLE);
    }
  }

  @Override
  public void onDestroyView()
  {
    super.onDestroyView();
    mBinding = null;
  }

  public void onClick(View view)
  {
    SettingsActivity.launch(requireContext(), mSettingShortcut);
  }
}
