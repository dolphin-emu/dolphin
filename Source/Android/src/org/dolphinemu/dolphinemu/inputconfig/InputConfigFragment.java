package org.dolphinemu.dolphinemu.inputconfig;

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

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.gamelist.GameListActivity;

/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */
public final class InputConfigFragment extends Fragment
		implements GameListActivity.OnGameConfigListener
{
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
		{
			return input.getDescriptor();
		}
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
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		List<InputConfigItem> Input = new ArrayList<InputConfigItem>();
		Input.add(new InputConfigItem(getString(R.string.button_a), "Android-InputA"));
		Input.add(new InputConfigItem(getString(R.string.button_b), "Android-InputB"));
		Input.add(new InputConfigItem(getString(R.string.button_start), "Android-InputStart"));
		Input.add(new InputConfigItem(getString(R.string.button_x), "Android-InputX"));
		Input.add(new InputConfigItem(getString(R.string.button_y), "Android-InputY"));
		Input.add(new InputConfigItem(getString(R.string.button_z), "Android-InputZ"));
		Input.add(new InputConfigItem(getString(R.string.dpad_up), "Android-DPadUp"));
		Input.add(new InputConfigItem(getString(R.string.dpad_down), "Android-DPadDown"));
		Input.add(new InputConfigItem(getString(R.string.dpad_left), "Android-DPadLeft"));
		Input.add(new InputConfigItem(getString(R.string.dpad_right), "Android-DPadRight"));
		Input.add(new InputConfigItem(getString(R.string.main_stick_up), "Android-MainUp"));
		Input.add(new InputConfigItem(getString(R.string.main_stick_down), "Android-MainDown"));
		Input.add(new InputConfigItem(getString(R.string.main_stick_left), "Android-MainLeft"));
		Input.add(new InputConfigItem(getString(R.string.main_stick_right), "Android-MainRight"));
		Input.add(new InputConfigItem(getString(R.string.c_stick_up), "Android-CStickUp"));
		Input.add(new InputConfigItem(getString(R.string.c_stick_down), "Android-CStickDown"));
		Input.add(new InputConfigItem(getString(R.string.c_stick_left), "Android-CStickLeft"));
		Input.add(new InputConfigItem(getString(R.string.c_stick_right), "Android-CStickRight"));
		Input.add(new InputConfigItem(getString(R.string.trigger_left), "Android-InputL"));
		Input.add(new InputConfigItem(getString(R.string.trigger_right), "Android-InputR"));

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
						Toast.makeText(m_activity, getString(R.string.not_drawing_onscreen_controls), Toast.LENGTH_SHORT).show();
						newBind = "False";
					}
					else
					{
						Toast.makeText(m_activity, getString(R.string.drawing_onscreen_controls), Toast.LENGTH_SHORT).show();
						newBind = "True";
					}
					adapter.remove(o);
					o.setBind(newBind);
					adapter.insert(o, position);
					break;
					
				default: // gamepad controls
				    
					Toast.makeText(m_activity, getString(R.string.press_button_to_config, o.getName()), Toast.LENGTH_SHORT).show();
					configPosition = position;
					Configuring = true;
					firstEvent = true;
					break;
			}
		}
	};

	private static ArrayList<Float> m_values = new ArrayList<Float>();

	private void AssignBind(String bind)
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
				
				for (InputDevice.MotionRange range : motions)
				{
					m_values.add(event.getAxisValue(range.getAxis()));
				}
				
				firstEvent = false;
			}
			else
			{
				for (int a = 0; a < motions.size(); ++a)
				{
					InputDevice.MotionRange range = motions.get(a);
					
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
		Log.w("InputConfigFragment", "Got Event " + event.getAction());
		switch (event.getAction())
		{
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
	public void onAttach(Activity activity)
	{
		super.onAttach(activity);

		// This makes sure that the container activity has implemented
		// the callback interface. If not, it throws an exception
		try
		{
			m_activity = activity;
		}
		catch (ClassCastException e)
		{
			throw new ClassCastException(activity.toString()
					+ " must implement OnGameListZeroListener");
		}
	}
}
