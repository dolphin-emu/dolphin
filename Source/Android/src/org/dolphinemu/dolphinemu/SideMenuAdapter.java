package org.dolphinemu.dolphinemu;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.TextView;

import java.util.List;

public class SideMenuAdapter extends ArrayAdapter<SideMenuItem>{

	private Context c;
	private int id;
	private List<SideMenuItem>items;
	
	public SideMenuAdapter(Context context, int textViewResourceId,
			List<SideMenuItem> objects) {
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
      public View getView(int position, View convertView, ViewGroup parent) {
              View v = convertView;
              if (v == null) {
                  LayoutInflater vi = (LayoutInflater)c.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
                  v = vi.inflate(id, null);
              }
              final SideMenuItem o = items.get(position);
              if (o != null) {
                      TextView t1 = (TextView) v.findViewById(R.id.SideMenuTitle);
                      
                      if(t1!=null)
                      	t1.setText(o.getName());
              }
              return v;
      }



}

