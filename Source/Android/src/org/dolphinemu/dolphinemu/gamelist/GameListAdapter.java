/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.gamelist;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.TextView;

import org.dolphinemu.dolphinemu.R;

/**
 * The adapter backing the game list.
 * <p>
 * Responsible for handling each game list item individually.
 */
public final class GameListAdapter extends ArrayAdapter<GameListItem>
{
	private final Context context;
	private final int id;

	/**
	 * Constructor
	 * 
	 * @param context    The current {@link Context}.
	 * @param resourceId The resource ID for a layout file containing a layout to use when instantiating views.
	 */
	public GameListAdapter(Context context, int resourceId)
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
		}

		final GameListItem item = getItem(position);
		if (item != null)
		{
			TextView title    = (TextView) convertView.findViewById(R.id.ListItemTitle);
			TextView subtitle = (TextView) convertView.findViewById(R.id.ListItemSubTitle);
			ImageView icon    = (ImageView) convertView.findViewById(R.id.ListItemIcon);

			if (title != null)
				title.setText(item.getName());

			if (subtitle != null)
				subtitle.setText(item.getData());

			if (icon != null)
			{
				icon.setImageBitmap(item.getImage());
				icon.getLayoutParams().width = (int) ((860 / context.getResources().getDisplayMetrics().density) + 0.5);
				icon.getLayoutParams().height = (int)((340 / context.getResources().getDisplayMetrics().density) + 0.5);
			}
		}

		// Make every other game in the list grey
		if (position % 2 == 1)
			convertView.setBackgroundColor(0xFFE3E3E3);

		return convertView;
	}
}

