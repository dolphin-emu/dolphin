package org.dolphinemu.dolphinemu.fragments;

import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.util.SparseIntArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.GridLayout;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;

public final class SaveLoadStateFragment extends Fragment implements View.OnClickListener
{
	public enum SaveOrLoad
	{
		SAVE, LOAD
	}

	private static final String KEY_SAVEORLOAD = "saveorload";
	private static SparseIntArray saveButtonsActionsMap = new SparseIntArray();
	static {
		saveButtonsActionsMap.append(R.id.loadsave_state_button_1, EmulationActivity.MENU_ACTION_SAVE_SLOT1);
		saveButtonsActionsMap.append(R.id.loadsave_state_button_2, EmulationActivity.MENU_ACTION_SAVE_SLOT2);
		saveButtonsActionsMap.append(R.id.loadsave_state_button_3, EmulationActivity.MENU_ACTION_SAVE_SLOT3);
		saveButtonsActionsMap.append(R.id.loadsave_state_button_4, EmulationActivity.MENU_ACTION_SAVE_SLOT4);
		saveButtonsActionsMap.append(R.id.loadsave_state_button_5, EmulationActivity.MENU_ACTION_SAVE_SLOT5);
		saveButtonsActionsMap.append(R.id.loadsave_state_button_6, EmulationActivity.MENU_ACTION_SAVE_SLOT6);
	}
	private static SparseIntArray loadButtonsActionsMap = new SparseIntArray();
	static {
		loadButtonsActionsMap.append(R.id.loadsave_state_button_1, EmulationActivity.MENU_ACTION_LOAD_SLOT1);
		loadButtonsActionsMap.append(R.id.loadsave_state_button_2, EmulationActivity.MENU_ACTION_LOAD_SLOT2);
		loadButtonsActionsMap.append(R.id.loadsave_state_button_3, EmulationActivity.MENU_ACTION_LOAD_SLOT3);
		loadButtonsActionsMap.append(R.id.loadsave_state_button_4, EmulationActivity.MENU_ACTION_LOAD_SLOT4);
		loadButtonsActionsMap.append(R.id.loadsave_state_button_5, EmulationActivity.MENU_ACTION_LOAD_SLOT5);
		loadButtonsActionsMap.append(R.id.loadsave_state_button_6, EmulationActivity.MENU_ACTION_LOAD_SLOT6);
	}
	private SaveOrLoad mSaveOrLoad;

	public static SaveLoadStateFragment newInstance(SaveOrLoad saveOrLoad)
	{
		SaveLoadStateFragment fragment = new SaveLoadStateFragment();

		Bundle arguments = new Bundle();
		arguments.putSerializable(KEY_SAVEORLOAD, saveOrLoad);
		fragment.setArguments(arguments);

		return fragment;
	}

	@Override
	public void onCreate(@Nullable Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		mSaveOrLoad = (SaveOrLoad) getArguments().getSerializable(KEY_SAVEORLOAD);
	}

	@Nullable
	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		View rootView = inflater.inflate(R.layout.fragment_saveload_state, container, false);

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
		int action = 0;
		switch(mSaveOrLoad)
		{
			case SAVE:
				action = saveButtonsActionsMap.get(button.getId(), -1);
				break;
			case LOAD:
				action = loadButtonsActionsMap.get(button.getId(), -1);
		}
		if (action >= 0)
		{
			((EmulationActivity) getActivity()).handleMenuAction(action);
		}
	}
}
