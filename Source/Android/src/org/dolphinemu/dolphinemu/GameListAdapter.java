package org.dolphinemu.dolphinemu;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.TextView;

import java.util.List;

public class GameListAdapter extends ArrayAdapter<GameListItem>{

    private Context c;
    private int id;
    private List<GameListItem>items;

    public GameListAdapter(Context context, int textViewResourceId, List<GameListItem> objects) {
        super(context, textViewResourceId, objects);
        c = context;
        id = textViewResourceId;
        items = objects;
    }

    public GameListItem getItem(int i)
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

        final GameListItem o = items.get(position);
        if (o != null) {
            TextView t1 = (TextView) v.findViewById(R.id.GameItemTitle);
            TextView t2 = (TextView) v.findViewById(R.id.GameItemSubText);
            ImageView v1 = (ImageView) v.findViewById(R.id.GameItemIcon);

            if(t1!=null)
                t1.setText(o.getName());
            if(t2!=null)
                t2.setText(o.getData());
            if(v1!=null)
                v1.setImageBitmap(o.getImage());
          }
        return v;
    }
}

