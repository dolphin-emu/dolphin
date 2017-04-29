package org.dolphinemu.dolphinemu.fragments;

import android.app.Fragment;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.util.SparseIntArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.dolphinemu.dolphinemu.BuildConfig;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;

public final class MenuFragment extends Fragment implements View.OnClickListener
{
	public static final String FRAGMENT_TAG = BuildConfig.APPLICATION_ID + ".ingame_menu";
	public static final int FRAGMENT_ID = R.layout.fragment_ingame_menu;
	private TextView mTitleText;
	private static SparseIntArray buttonsActionsMap = new SparseIntArray();
	static {
		buttonsActionsMap.append(R.id.menu_take_screenshot, EmulationActivity.MENU_ACTION_TAKE_SCREENSHOT);
		buttonsActionsMap.append(R.id.menu_quicksave, EmulationActivity.MENU_ACTION_QUICK_SAVE);
		buttonsActionsMap.append(R.id.menu_quickload, EmulationActivity.MENU_ACTION_QUICK_LOAD);
		buttonsActionsMap.append(R.id.menu_emulation_save_root, EmulationActivity.MENU_ACTION_SAVE_ROOT);
		buttonsActionsMap.append(R.id.menu_emulation_load_root, EmulationActivity.MENU_ACTION_LOAD_ROOT);
		buttonsActionsMap.append(R.id.menu_refresh_wiimotes, EmulationActivity.MENU_ACTION_REFRESH_WIIMOTES);
		buttonsActionsMap.append(R.id.menu_exit, EmulationActivity.MENU_ACTION_EXIT);
	}

	@Nullable
	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		View rootView = inflater.inflate(FRAGMENT_ID, container, false);

		LinearLayout options = (LinearLayout) rootView.findViewById(R.id.layout_options);
		for (int childIndex = 0; childIndex < options.getChildCount(); childIndex++)
		{
			Button button = (Button) options.getChildAt(childIndex);

			button.setOnClickListener(this);
		}

		mTitleText = (TextView) rootView.findViewById(R.id.text_game_title);

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

	public void setTitleText(String title)
	{
		mTitleText.setText(title);
	}
}
