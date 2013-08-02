package org.dolphinemu.dolphinemu;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.TextView;

import java.util.List;

/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */
public class InputConfigAdapter extends ArrayAdapter<InputConfigItem> {
	private Context c;
	private int id;
	private List<InputConfigItem> items;

	public InputConfigAdapter(Context context, int textViewResourceId,
	                       List<InputConfigItem> objects) {
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
	public View getView(int position, View convertView, ViewGroup parent) {
		View v = convertView;
		if (v == null) {
			LayoutInflater vi = (LayoutInflater)c.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
			v = vi.inflate(id, null);
		}
		final InputConfigItem o = items.get(position);
		if (o != null) {
			TextView t1 = (TextView) v.findViewById(R.id.FolderTitle);
			TextView t2 = (TextView) v.findViewById(R.id.FolderSubTitle);

			if(t1!=null)
				t1.setText(o.getName());
			if(t2!=null)
				t2.setText(o.getBind());
		}
		return v;
	}



}
