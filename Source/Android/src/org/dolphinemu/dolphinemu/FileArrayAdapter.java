package org.dolphinemu.dolphinemu;

import java.util.List;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.TextView;

public class FileArrayAdapter extends ArrayAdapter<Option>{

	private Context c;
	private int id;
	private List<Option>items;
	
	public FileArrayAdapter(Context context, int textViewResourceId,
			List<Option> objects) {
		super(context, textViewResourceId, objects);
		c = context;
		id = textViewResourceId;
		items = objects;
	}
	public Option getItem(int i)
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
              final Option o = items.get(position);
              if (o != null) {
                      TextView t1 = (TextView) v.findViewById(R.id.TextView01);
                      TextView t2 = (TextView) v.findViewById(R.id.TextView02);
                      
                      if(t1!=null)
                      	t1.setText(o.getName());
                      if(t2!=null)
                      	t2.setText(o.getData());
                      
              }
              return v;
      }



}

