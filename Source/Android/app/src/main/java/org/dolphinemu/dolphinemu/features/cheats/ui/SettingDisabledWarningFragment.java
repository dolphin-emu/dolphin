// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import org.dolphinemu.dolphinemu.R;
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

  public SettingDisabledWarningFragment(AbstractBooleanSetting setting, MenuTag settingShortcut,
          int text)
  {
    mSetting = setting;
    mSettingShortcut = settingShortcut;
    mText = text;
  }

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
          @Nullable Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.fragment_cheat_warning, container, false);
  }

  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    mView = view;

    TextView textView = view.findViewById(R.id.text_warning);
    textView.setText(mText);

    Button settingsButton = view.findViewById(R.id.button_settings);
    settingsButton.setOnClickListener(this);

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

  public void onClick(View view)
  {
    SettingsActivity.launch(requireContext(), mSettingShortcut);
  }
}
