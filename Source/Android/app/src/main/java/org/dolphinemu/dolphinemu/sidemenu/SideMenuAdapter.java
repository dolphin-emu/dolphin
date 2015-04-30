/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.sidemenu;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.TextView;

import java.util.List;

import org.dolphinemu.dolphinemu.R;

/**
 * Adapter that backs the sidebar menu.
 * <p>
 * Responsible for handling the elements of each sidebar item.
 */
public final class SideMenuAdapter extends ArrayAdapter<SideMenuItem>
{
	private final Context context;
	private final int id;
	private final List<SideMenuItem>items;

	/**
	 * Constructor
	 * 
	 * @param context    The current {@link Context}.
	 * @param resourceId The resource ID for a layout file containing a layout to use when instantiating views.
	 * @param objects    The objects to represent in the {@link ListView}.
	 */
	public SideMenuAdapter(Context context, int resourceId, List<SideMenuItem> objects)
	{
		super(context, resourceId, objects);

		this.context = context;
		this.id = resourceId;
		this.items = objects;
	}

	@Override
	public SideMenuItem getItem(int i)
	{
		return items.get(i);
	}

	@Override
	public View getView(int position, View convertView, ViewGroup parent)
	{
		if (convertView == null)
		{
			LayoutInflater vi = LayoutInflater.from(context);
			convertView = vi.inflate(id, null);
		}

		final SideMenuItem item = items.get(position);
		if (item != null)
		{
			TextView title = (TextView) convertView.findViewById(R.id.SideMenuTitle);

			if (title != null)
				title.setText(item.getName());
		}

		return convertView;
	}
}

