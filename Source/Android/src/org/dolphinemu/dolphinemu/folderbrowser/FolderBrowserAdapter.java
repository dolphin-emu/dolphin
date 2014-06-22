/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.folderbrowser;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
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
	// ViewHolder which is used to hold onto
	// items within a listview. This is done
	// so that findViewById is not needed to
	// be excessively called over and over.
	private static final class ViewHolder
	{
		TextView title;
		TextView subtitle;
		ImageView icon;
	}

	private final Context context;
	private final int id;
	private ViewHolder viewHolder;

	/**
	 * Constructor
	 * 
	 * @param context    The current {@link Context}.
	 * @param resourceId The resource ID for a layout file containing a layout to use when instantiating views.
	 */
	public FolderBrowserAdapter(Context context, int resourceId)
	{
		super(context, resourceId);

		this.context = context;
		this.id = resourceId;
	}

	@Override
	public View getView(int position, View convertView, ViewGroup parent)
	{
		if (convertView == null)
		{
			LayoutInflater vi = LayoutInflater.from(context);
			convertView = vi.inflate(id, parent, false);

			// Initialize the ViewHolder and store it.
			viewHolder = new ViewHolder();
			viewHolder.title    = (TextView)  convertView.findViewById(R.id.BrowserItemTitle);
			viewHolder.subtitle = (TextView)  convertView.findViewById(R.id.BrowserItemSubTitle);
			viewHolder.icon     = (ImageView) convertView.findViewById(R.id.BrowserItemIcon);
			convertView.setTag(viewHolder);
		}
		else // Can recover the holder.
		{
			viewHolder = (ViewHolder) convertView.getTag();
		}

		final FolderBrowserItem item = getItem(position);
		if (item != null)
		{
			if (viewHolder.title != null)
			{
				viewHolder.title.setText(item.getName());
			}

			if (viewHolder.subtitle != null)
			{
				// Remove the subtitle for all folders, except for the parent directory folder.
				if (item.isDirectory() && !item.getSubtitle().equals(context.getString(R.string.parent_directory)))
				{
					viewHolder.subtitle.setVisibility(View.GONE);
				}
				else
				{
					viewHolder.subtitle.setVisibility(View.VISIBLE);
					viewHolder.subtitle.setText(item.getSubtitle());
				}
			}

			if (viewHolder.icon != null)
			{
				if (item.isDirectory())
				{
					viewHolder.icon.setImageResource(R.drawable.ic_menu_folder);
				}
				else
				{
					viewHolder.icon.setImageResource(R.drawable.ic_menu_file);
				}
			}
		}
		return convertView;
	}
}
