package org.dolphinemu.dolphinemu.inputconfig;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.TextView;

import java.util.List;

import org.dolphinemu.dolphinemu.R;

/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */
public final class InputConfigAdapter extends ArrayAdapter<InputConfigItem>
{
	private final Context c;
	private final int id;
	private final List<InputConfigItem> items;

	public InputConfigAdapter(Context context, int textViewResourceId, List<InputConfigItem> objects)
	{
		super(context, textViewResourceId, objects);
		c = context;
		id = textViewResourceId;
		items = objects;
	}

	public InputConfigItem getItem(int i)
	{
		return items.get(i);
	}
	
	@Override
	public View getView(int position, View convertView, ViewGroup parent)
	{
		View v = convertView;
		if (v == null)
		{
			LayoutInflater vi = (LayoutInflater)c.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
			v = vi.inflate(id, parent, false);
		}
		
		final InputConfigItem item = items.get(position);
		if (item != null)
		{
			TextView title    = (TextView) v.findViewById(R.id.FolderTitle);
			TextView subtitle = (TextView) v.findViewById(R.id.FolderSubTitle);

			if(title != null)
			   title.setText(item.getName());
			
			if(subtitle != null)
			   subtitle.setText(item.getBind());
		}
		
		return v;
	}
}
