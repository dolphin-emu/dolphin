// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivity;

public class CheatWarningFragment extends Fragment implements View.OnClickListener
{
  private View mView;

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
      boolean cheatsEnabled = BooleanSetting.MAIN_ENABLE_CHEATS.getBoolean(settings);
      mView.setVisibility(cheatsEnabled ? View.GONE : View.VISIBLE);
    }
  }

  public void onClick(View view)
  {
    SettingsActivity.launch(requireContext(), MenuTag.CONFIG_GENERAL);
  }
}
