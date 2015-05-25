/**
 * Copyright 2014 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.about;

import java.util.List;

import org.dolphinemu.dolphinemu.R;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.TextView;

/**
 * {@link ArrayAdapter} subclass specifically for the
 * information fragments within the about menu.
 */
final class AboutInfoFragmentAdapter extends ArrayAdapter<AboutFragmentItem>
{
	private final int id;
	private final List<AboutFragmentItem> items;

	public AboutInfoFragmentAdapter(Context ctx, int id, List<AboutFragmentItem> items)
	{
		super(ctx, id, items);

		this.id = id;
		this.items = items;
	}

	@Override
	public AboutFragmentItem getItem(int index)
	{
		return items.get(index);
	}

	@Override
	public View getView(int position, View convertView, ViewGroup parent)
	{
		if (convertView == null)
		{
			LayoutInflater vi = LayoutInflater.from(getContext());
			convertView = vi.inflate(id, parent, false);
		}

		final AboutFragmentItem item = items.get(position);
		if (item != null)
		{
			TextView title    = (TextView) convertView.findViewById(R.id.AboutItemTitle);
			TextView subtitle = (TextView) convertView.findViewById(R.id.AboutItemSubTitle);

			if (title != null)
				title.setText(item.getTitle());

			if (subtitle != null)
				subtitle.setText(item.getSubTitle());
		}

		return convertView;
	}
}
