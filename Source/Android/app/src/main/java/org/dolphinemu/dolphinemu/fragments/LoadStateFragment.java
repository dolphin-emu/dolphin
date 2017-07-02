package org.dolphinemu.dolphinemu.fragments;

import android.app.Fragment;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.util.SparseIntArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.GridLayout;

import org.dolphinemu.dolphinemu.BuildConfig;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;

public final class LoadStateFragment extends Fragment implements View.OnClickListener
{
	public static final String FRAGMENT_TAG = BuildConfig.APPLICATION_ID + ".load_state";
	public static final int FRAGMENT_ID = R.layout.fragment_state_load;

	private static SparseIntArray buttonsActionsMap = new SparseIntArray();
	static {
		buttonsActionsMap.append(R.id.menu_emulation_load_1, EmulationActivity.MENU_ACTION_LOAD_SLOT1);
		buttonsActionsMap.append(R.id.menu_emulation_load_2, EmulationActivity.MENU_ACTION_LOAD_SLOT2);
		buttonsActionsMap.append(R.id.menu_emulation_load_3, EmulationActivity.MENU_ACTION_LOAD_SLOT3);
		buttonsActionsMap.append(R.id.menu_emulation_load_4, EmulationActivity.MENU_ACTION_LOAD_SLOT4);
		buttonsActionsMap.append(R.id.menu_emulation_load_5, EmulationActivity.MENU_ACTION_LOAD_SLOT5);
		buttonsActionsMap.append(R.id.menu_emulation_load_6, EmulationActivity.MENU_ACTION_LOAD_SLOT6);
	}

	public static LoadStateFragment newInstance()
	{
		LoadStateFragment fragment = new LoadStateFragment();

		// TODO Add any appropriate arguments to this fragment.

		return fragment;
	}

	@Nullable
	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		View rootView = inflater.inflate(FRAGMENT_ID, container, false);

		GridLayout grid = (GridLayout) rootView.findViewById(R.id.grid_state_slots);
		for (int childIndex = 0; childIndex < grid.getChildCount(); childIndex++)
		{
			Button button = (Button) grid.getChildAt(childIndex);

			button.setOnClickListener(this);
		}

		// So that item clicked to start this Fragment is no longer the focused item.
		grid.requestFocus();

		return rootView;
	}

	@SuppressWarnings("WrongConstant")
	@Override
	public void onClick(View button)
	{
		int action = buttonsActionsMap.get(button.getId(), -1);
		if (action >= 0)
		{
			((EmulationActivity) getActivity()).handleMenuAction(action);
		}
	}
}
