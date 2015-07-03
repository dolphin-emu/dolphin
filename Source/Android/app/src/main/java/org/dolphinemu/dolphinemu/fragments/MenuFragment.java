package org.dolphinemu.dolphinemu.fragments;

import android.app.Fragment;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;

public final class MenuFragment extends Fragment implements View.OnClickListener
{
	@Nullable
	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		LinearLayout rootView = (LinearLayout) inflater.inflate(R.layout.fragment_ingame_menu, container, false);

		for (int childIndex = 0; childIndex < rootView.getChildCount(); childIndex++)
		{
			Button button = (Button) rootView.getChildAt(childIndex);

			button.setOnClickListener(this);
		}

		return rootView;
	}

	@Override
	public void onClick(View button)
	{
		((EmulationActivity) getActivity()).onMenuItemClicked(button.getId());
	}
}
