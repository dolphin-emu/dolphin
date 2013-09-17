/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu;

import android.app.Activity;
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

import org.dolphinemu.dolphinemu.settings.VideoSettingsFragment;

/**
 * Represents the about screen.
 */
public final class AboutFragment extends ListFragment
{
	private static Activity m_activity;

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		View rootView = inflater.inflate(R.layout.gamelist_listview, container, false);
		ListView mMainList = (ListView) rootView.findViewById(R.id.gamelist);

		String yes = getString(R.string.yes);
		String no = getString(R.string.no);

		List<AboutFragmentItem> Input = new ArrayList<AboutFragmentItem>();
		Input.add(new AboutFragmentItem(getString(R.string.build_revision), NativeLibrary.GetVersionString()));
		Input.add(new AboutFragmentItem(getString(R.string.supports_gles3), VideoSettingsFragment.SupportsGLES3() ? yes : no));

		AboutFragmentAdapter adapter = new AboutFragmentAdapter(m_activity, R.layout.about_layout, Input);
		mMainList.setAdapter(adapter);
		mMainList.setEnabled(false);  // Makes the list view non-clickable.

		return mMainList;
	}

	@Override
	public void onAttach(Activity activity)
	{
		super.onAttach(activity);

		// Cache the activity instance.
		m_activity = activity;
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
			View v = convertView;
			if (v == null)
			{
				LayoutInflater vi = (LayoutInflater) ctx.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
				v = vi.inflate(id, parent, false);
			}
			
			final AboutFragmentItem item = items.get(position);
			if (item != null)
			{
				TextView title    = (TextView) v.findViewById(R.id.AboutItemTitle);
				TextView subtitle = (TextView) v.findViewById(R.id.AboutItemSubTitle);

				if (title != null)
					title.setText(item.getTitle());

				if (subtitle != null)
					subtitle.setText(item.getSubTitle());
			}

			return v;
		}
	}
}
