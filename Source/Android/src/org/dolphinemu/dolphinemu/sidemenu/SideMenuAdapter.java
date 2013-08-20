package org.dolphinemu.dolphinemu.sidemenu;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.TextView;

import java.util.List;

import org.dolphinemu.dolphinemu.R;

public final class SideMenuAdapter extends ArrayAdapter<SideMenuItem>
{
	private final Context c;
	private final int id;
	private final List<SideMenuItem>items;
	
	public SideMenuAdapter(Context context, int textViewResourceId, List<SideMenuItem> objects)
	{
		super(context, textViewResourceId, objects);
		c = context;
		id = textViewResourceId;
		items = objects;
	}
	
	public SideMenuItem getItem(int i)
	{
		 return items.get(i);
	}
	
	@Override
    public View getView(int position, View convertView, ViewGroup parent)
	{
        View v = convertView;
         if (v == null)
         {
             LayoutInflater vi = (LayoutInflater)c.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
             v = vi.inflate(id, null);
         }
          
         final SideMenuItem item = items.get(position);
         if (item != null)
         {
             TextView title = (TextView) v.findViewById(R.id.SideMenuTitle);
              
             if(title != null)
             	title.setText(item.getName());
         }
          
         return v;
	}
}

