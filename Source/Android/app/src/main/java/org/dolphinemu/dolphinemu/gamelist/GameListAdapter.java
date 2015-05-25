/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.gamelist;

import android.content.Context;
import android.graphics.Bitmap;
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
	private static final float BYTES_PER_GIB = 1024 * 1024 * 1024;
	private static final float BYTES_PER_MIB = 1024 * 1024;

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
	public View getView(int position, View gameView, ViewGroup parent)
	{
		ViewHolder holder;

		// If getView() was called without passing in a recycled view,
		if (gameView == null)
		{
			// Inflate a new view using the appropriate XML layout.
			LayoutInflater inflater = LayoutInflater.from(context);
			gameView = inflater.inflate(id, parent, false);

			// Instantiate a holder to contain references to the game's views.
			holder = new ViewHolder();
			holder.title    = (TextView) gameView.findViewById(R.id.GameListItemTitle);
			holder.subtitle = (TextView) gameView.findViewById(R.id.GameListItemSubTitle);
			holder.icon    = (ImageView) gameView.findViewById(R.id.GameListItemIcon);

			// Attach this list of references to the view.
			gameView.setTag(holder);
		}
		// If we do have a recycled view, we can use the references it already contains.
		else
		{
			holder = (ViewHolder) gameView.getTag();
		}

		// Get a reference to the game represented by this row.
		final GameListItem item = getItem(position);

		// Whether this row's view is newly created or not, set the children to contain the game's data.
		if (item != null)
		{
			holder.title.setText(item.getName());

			Bitmap icon = item.getImage();

			if (icon != null)
			{
				holder.icon.setImageBitmap(icon);
			}
			else
			{
				holder.icon.setImageResource(R.drawable.no_banner);
			}

			float fileSize = item.getFilesize() / BYTES_PER_GIB;

			String subtitle;

			if (fileSize >= 1.0f)
			{
				subtitle = String.format(context.getString(R.string.file_size_gib), fileSize);
			}
			else
			{
				fileSize = item.getFilesize() / BYTES_PER_MIB;
				subtitle = String.format(context.getString(R.string.file_size_mib), fileSize);
			}

			holder.subtitle.setText(subtitle);
		}

		// Make every other game in the list grey
		if (position % 2 == 1)
			gameView.setBackgroundColor(0xFFE3E3E3);

		return gameView;
	}

	private final class ViewHolder
	{
		public TextView title;
		public TextView subtitle;
		public ImageView icon;
	}
}

