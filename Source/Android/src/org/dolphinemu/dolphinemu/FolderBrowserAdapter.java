package org.dolphinemu.dolphinemu;

import android.content.Context;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.TextView;

import java.util.List;

public class FolderBrowserAdapter extends ArrayAdapter<FolderBrowserItem>{

	private Context c;
	private int id;
	private List<FolderBrowserItem> items;

	public FolderBrowserAdapter(Context context, int textViewResourceId, List<FolderBrowserItem> objects) {
		super(context, textViewResourceId, objects);
		c = context;
		id = textViewResourceId;
		items = objects;
	}

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
			LayoutInflater vi = (LayoutInflater) c.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
			v = vi.inflate(id, parent, false);
		}
		
		final FolderBrowserItem item = items.get(position);
		if (item != null)
		{
		    ImageView iconView = (ImageView) v.findViewById(R.id.ImageIcon);
			TextView mainText = (TextView) v.findViewById(R.id.FolderTitle);
			TextView subtitleText = (TextView) v.findViewById(R.id.FolderSubTitle);

			if(mainText != null)
			{
				mainText.setText(item.getName());
				
				if (!item.isValidItem())
				{
					mainText.setTextColor(0xFFFF0000);
				}
			}
			
			if(subtitleText != null)
			{
				subtitleText.setText(item.getSubtitle());
			}
			
			if (iconView != null)
			{
			    if (item.isDirectory())
			    {
			        iconView.setImageResource(R.drawable.ic_menu_folder);
			    }
			    else
			    {
			        iconView.setImageResource(R.drawable.ic_menu_file);
			    }
			}

		}
		return v;
	}
}
