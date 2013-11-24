/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu;

import android.app.ListFragment;
import android.content.Context;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

import org.dolphinemu.dolphinemu.settings.video.VideoSettingsFragment;

/**
 * Represents the about screen.
 */
public final class AboutFragment extends ListFragment
{
	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		ListView rootView = (ListView) inflater.inflate(R.layout.gamelist_listview, container, false);

		final String yes = getString(R.string.yes);
		final String no = getString(R.string.no);

		List<AboutFragmentItem> Input = new ArrayList<AboutFragmentItem>();
		Input.add(new AboutFragmentItem(getString(R.string.build_revision), NativeLibrary.GetVersionString()));
		Input.add(new AboutFragmentItem(getString(R.string.supports_gles3), VideoSettingsFragment.SupportsGLES3() ? yes : no));
		Input.add(new AboutFragmentItem(getString(R.string.supports_neon),  NativeLibrary.SupportsNEON() ? yes : no));

		AboutFragmentAdapter adapter = new AboutFragmentAdapter(getActivity(), R.layout.about_layout, Input);
		rootView.setAdapter(adapter);
		rootView.setEnabled(false);  // Makes the list view non-clickable.

		return rootView;
	}

	// Represents an item in the AboutFragment.
	private static final class AboutFragmentItem
	{
		private final String title;
		private final String subtitle;

		public AboutFragmentItem(String title, String subtitle)
		{
			this.title = title;
			this.subtitle = subtitle;
		}
		
		public String getTitle()
		{
			return title;
		}
		
		public String getSubTitle()
		{
			return subtitle;
		}
	}

	// The adapter that manages the displaying of items in this AboutFragment.
	private static final class AboutFragmentAdapter extends ArrayAdapter<AboutFragmentItem>
	{
		private final Context ctx;
		private final int id;
		private final List<AboutFragmentItem> items;

		public AboutFragmentAdapter(Context ctx, int id, List<AboutFragmentItem> items)
		{
			super(ctx, id, items);

			this.ctx = ctx;
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
				LayoutInflater vi = LayoutInflater.from(ctx);
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
}
