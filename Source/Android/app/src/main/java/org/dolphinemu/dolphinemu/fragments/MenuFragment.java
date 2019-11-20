package org.dolphinemu.dolphinemu.fragments;

import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import android.util.SparseIntArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.utils.SettingsFile;

public final class MenuFragment extends Fragment implements View.OnClickListener
{
  private static final String KEY_TITLE = "title";
  private static SparseIntArray buttonsActionsMap = new SparseIntArray();

  static
  {
    buttonsActionsMap
            .append(R.id.menu_take_screenshot, EmulationActivity.MENU_ACTION_TAKE_SCREENSHOT);
    buttonsActionsMap.append(R.id.menu_quicksave, EmulationActivity.MENU_ACTION_QUICK_SAVE);
    buttonsActionsMap.append(R.id.menu_quickload, EmulationActivity.MENU_ACTION_QUICK_LOAD);
    buttonsActionsMap
            .append(R.id.menu_emulation_save_root, EmulationActivity.MENU_ACTION_SAVE_ROOT);
    buttonsActionsMap
            .append(R.id.menu_emulation_load_root, EmulationActivity.MENU_ACTION_LOAD_ROOT);
    buttonsActionsMap
            .append(R.id.menu_refresh_wiimotes, EmulationActivity.MENU_ACTION_REFRESH_WIIMOTES);
    buttonsActionsMap.append(R.id.menu_change_disc, EmulationActivity.MENU_ACTION_CHANGE_DISC);
    buttonsActionsMap.append(R.id.menu_exit, EmulationActivity.MENU_ACTION_EXIT);
  }

  public static MenuFragment newInstance(String title)
  {
    MenuFragment fragment = new MenuFragment();

    Bundle arguments = new Bundle();
    arguments.putSerializable(KEY_TITLE, title);
    fragment.setArguments(arguments);

    return fragment;
  }

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
  {
    View rootView = inflater.inflate(R.layout.fragment_ingame_menu, container, false);

    LinearLayout options = (LinearLayout) rootView.findViewById(R.id.layout_options);

    BooleanSetting enableSaveStates =
            (BooleanSetting) ((EmulationActivity) getActivity()).getSettings()
                    .getSection(Settings.SECTION_INI_CORE)
                    .getSetting(SettingsFile.KEY_ENABLE_SAVE_STATES);

    if (enableSaveStates != null && enableSaveStates.getValue())
    {
      options.findViewById(R.id.menu_quicksave).setVisibility(View.VISIBLE);
      options.findViewById(R.id.menu_quickload).setVisibility(View.VISIBLE);
      options.findViewById(R.id.menu_emulation_save_root).setVisibility(View.VISIBLE);
      options.findViewById(R.id.menu_emulation_load_root).setVisibility(View.VISIBLE);
    }

    for (int childIndex = 0; childIndex < options.getChildCount(); childIndex++)
    {
      Button button = (Button) options.getChildAt(childIndex);

      button.setOnClickListener(this);
    }

    TextView titleText = rootView.findViewById(R.id.text_game_title);
    String title = getArguments().getString(KEY_TITLE);
    if (title != null)
    {
      titleText.setText(title);
    }

    return rootView;
  }

  @SuppressWarnings("WrongConstant")
  @Override
  public void onClick(View button)
  {
    int action = buttonsActionsMap.get(button.getId());
    if (action >= 0)
    {
      ((EmulationActivity) getActivity()).handleMenuAction(action);
    }
  }
}
