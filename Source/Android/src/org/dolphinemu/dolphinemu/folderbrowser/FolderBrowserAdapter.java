/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.folderbrowser;

import java.util.List;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

import org.dolphinemu.dolphinemu.R;

/**
 * The {@link ArrayAdapter} that backs the file browser.
 * <p>
 * This is responsible for correctly handling the display
 * of the items for the {@link FolderBrowser} UI.
 */
public final class FolderBrowserAdapter extends ArrayAdapter<FolderBrowserItem>
{
	private final Context context;
	private final int id;
	private final List<FolderBrowserItem> items;

	/**
	 * Constructor
	 * 
	 * @param context    The current {@link Context}.
	 * @param resourceId The resource ID for a layout file containing a layout to use when instantiating views.
	 * @param objects    The objects to represent in the {@link ListView}.
	 */
	public FolderBrowserAdapter(Context context, int resourceId, List<FolderBrowserItem> objects)
	{
		super(context, resourceId, objects);

		this.context = context;
		this.id = resourceId;
		this.items = objects;
	}

	@Override
	public FolderBrowserItem getItem(int i)
	{
		return items.get(i);
	}

	@Override
	public View getView(int position, View convertView, ViewGroup parent)
	{
		View v = convertView;
		if (v == null)
		{
			LayoutInflater vi = LayoutInflater.from(context);
			v = vi.inflate(id, parent, false);
		}

		final FolderBrowserItem item = items.get(position);
		if (item != null)
		{
			ImageView icon    = (ImageView) v.findViewById(R.id.ListItemIcon);
			TextView title    = (TextView) v.findViewById(R.id.ListItemTitle);
			TextView subtitle = (TextView) v.findViewById(R.id.ListItemSubTitle);

			if(title != null)
			{
				title.setText(item.getName());
			}

			if(subtitle != null)
			{
				// Remove the subtitle for all folders, except for the parent directory folder.
				if (item.isDirectory() && !item.getSubtitle().equals(context.getString(R.string.parent_directory)))
				{
					subtitle.setVisibility(View.GONE);
				}
				else
				{
					subtitle.setText(item.getSubtitle());
				}
			}

			if (icon != null)
			{
				if (item.isDirectory())
				{
					icon.setImageResource(R.drawable.ic_menu_folder);
				}
				else
				{
					icon.setImageResource(R.drawable.ic_menu_file);
				}
			}
		}
		return v;
	}
}
