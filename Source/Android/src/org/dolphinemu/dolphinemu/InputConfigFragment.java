package org.dolphinemu.dolphinemu;

import android.app.Activity;
import android.app.Fragment;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.*;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.Toast;

import java.util.ArrayList;
import java.util.List;

/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */
public class InputConfigFragment extends Fragment
		implements GameListActivity.OnGameConfigListener{
	private Activity m_activity;
	private ListView mDrawerList;
	private InputConfigAdapter adapter;
	private int configPosition = 0;
	boolean Configuring = false;
	boolean firstEvent = true;

	static public String getInputDesc(InputDevice input)
	{
		if (input == null)
			return "null"; // Happens when the inputdevice is from an unknown source
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN)
			return input.getDescriptor();
		else
		{
			List<InputDevice.MotionRange> motions = input.getMotionRanges();
			String fakeid = "";
			for (InputDevice.MotionRange range : motions)
				fakeid += range.getAxis();
			return fakeid;
		}
	}
	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
	                         Bundle savedInstanceState)
	{
		List<InputConfigItem> Input = new ArrayList<InputConfigItem>();
		int a = 0;

		Input.add(a++, new InputConfigItem("Draw on-screen controls", "Android-ScreenControls", "True"));
		Input.add(a++, new InputConfigItem("Button A", "Android-InputA"));
		Input.add(a++, new InputConfigItem("Button B", "Android-InputB"));
		Input.add(a++, new InputConfigItem("Button Start", "Android-InputStart"));
		Input.add(a++, new InputConfigItem("Button X", "Android-InputX"));
		Input.add(a++, new InputConfigItem("Button Y", "Android-InputY"));
		Input.add(a++, new InputConfigItem("Button Z", "Android-InputZ"));
		Input.add(a++, new InputConfigItem("D-Pad Up", "Android-DPadUp"));
		Input.add(a++, new InputConfigItem("D-Pad Down", "Android-DPadDown"));
		Input.add(a++, new InputConfigItem("D-Pad Left", "Android-DPadLeft"));
		Input.add(a++, new InputConfigItem("D-Pad Right", "Android-DPadRight"));
		Input.add(a++, new InputConfigItem("Main Stick Up", "Android-MainUp"));
		Input.add(a++, new InputConfigItem("Main Stick Down", "Android-MainDown"));
		Input.add(a++, new InputConfigItem("Main Stick Left", "Android-MainLeft"));
		Input.add(a++, new InputConfigItem("Main Stick Right", "Android-MainRight"));
		Input.add(a++, new InputConfigItem("C Stick Up", "Android-CStickUp"));
		Input.add(a++, new InputConfigItem("C Stick Down", "Android-CStickDown"));
		Input.add(a++, new InputConfigItem("C Stick Left", "Android-CStickLeft"));
		Input.add(a++, new InputConfigItem("C Stick Right", "Android-CStickRight"));
		Input.add(a++, new InputConfigItem("Trigger L", "Android-InputL"));
		Input.add(a++, new InputConfigItem("Trigger R", "Android-InputR"));

		adapter = new InputConfigAdapter(m_activity, R.layout.folderbrowser, Input);
		View rootView = inflater.inflate(R.layout.gamelist_listview, container, false);
		mDrawerList = (ListView) rootView.findViewById(R.id.gamelist);

		mDrawerList.setAdapter(adapter);
		mDrawerList.setOnItemClickListener(mMenuItemClickListener);
		return mDrawerList;
	}
	private AdapterView.OnItemClickListener mMenuItemClickListener = new AdapterView.OnItemClickListener()
	{
		public void onItemClick(AdapterView<?> parent, View view, int position, long id)
		{
			InputConfigItem o = adapter.getItem(position);
			switch(position)
			{
				case 0: // On screen controls
					String newBind;
					if (o.getBind().equals("True"))
					{
						Toast.makeText(m_activity, "Not Drawing on screen controls", Toast.LENGTH_SHORT).show();
						newBind = "False";
					}
					else
					{
						Toast.makeText(m_activity, "Drawing on screen controls", Toast.LENGTH_SHORT).show();
						newBind = "True";
					}
					adapter.remove(o);
					o.setBind(newBind);
					adapter.insert(o, position);
					break;
				default: // gamepad controls
					Toast.makeText(m_activity, "Press button to configure " + o.getName(), Toast.LENGTH_SHORT).show();
					configPosition = position;
					Configuring = true;
					firstEvent = true;
					break;
			}
		}
	};

	static ArrayList<Float> m_values = new ArrayList<Float>();

	void AssignBind(String bind)
	{
		InputConfigItem o = adapter.getItem(configPosition);
		adapter.remove(o);
		o.setBind(bind);
		adapter.insert(o, configPosition);
	}
	public InputConfigAdapter getAdapter()
	{
		return adapter;
	}
	// Called from GameListActivity
	public boolean onMotionEvent(MotionEvent event)
	{
		if (((event.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) == 0))
			return false;

		InputDevice input = event.getDevice();
		List<InputDevice.MotionRange> motions = input.getMotionRanges();
		if (Configuring)
		{
			if (firstEvent)
			{
				m_values.clear();
				for (InputDevice.MotionRange range : motions) {
					m_values.add(event.getAxisValue(range.getAxis()));
				}
				firstEvent = false;
			}
			else
			{
				for (int a = 0; a < motions.size(); ++a)
				{
					InputDevice.MotionRange range;
					range = motions.get(a);
					if (m_values.get(a) > (event.getAxisValue(range.getAxis()) + 0.5f))
					{
						AssignBind("Device '" + InputConfigFragment.getInputDesc(input) + "'-Axis " + range.getAxis() + "-");
						Configuring = false;
					}
					else if (m_values.get(a) < (event.getAxisValue(range.getAxis()) - 0.5f))
					{
						AssignBind("Device '" + InputConfigFragment.getInputDesc(input) + "'-Axis " + range.getAxis() + "+");
						Configuring = false;
					}
				}
			}
		}
		return true;
	}
	public boolean onKeyEvent(KeyEvent event)
	{
		Log.w("Dolphinemu", "Got Event " + event.getAction());
		switch (event.getAction()) {
			case KeyEvent.ACTION_DOWN:
			case KeyEvent.ACTION_UP:
				if (Configuring)
				{
					InputDevice input = event.getDevice();
					AssignBind("Device '" + InputConfigFragment.getInputDesc(input) + "'-Button " + event.getKeyCode());
					Configuring = false;
					return true;
				}
			default:
				break;
		}

		return false;
	}

	@Override
	public void onAttach(Activity activity) {
		super.onAttach(activity);

		// This makes sure that the container activity has implemented
		// the callback interface. If not, it throws an exception
		try {
			m_activity = activity;
		} catch (ClassCastException e) {
			throw new ClassCastException(activity.toString()
					+ " must implement OnGameListZeroListener");
		}
	}
}
