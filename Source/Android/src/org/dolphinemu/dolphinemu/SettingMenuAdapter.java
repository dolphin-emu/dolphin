package org.dolphinemu.dolphinemu;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.TextView;

import java.util.List;

/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

public class SettingMenuAdapter extends ArrayAdapter<SettingMenuItem>{

    private Context c;
    private int id;
    private List<SettingMenuItem>items;

    public SettingMenuAdapter(Context context, int textViewResourceId,
                           List<SettingMenuItem> objects) {
        super(context, textViewResourceId, objects);
        c = context;
        id = textViewResourceId;
        items = objects;
    }
    public SettingMenuItem getItem(int i)
    {
        return items.get(i);
    }
    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        View v = convertView;
        if (v == null) {
            LayoutInflater vi = (LayoutInflater)c.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
            v = vi.inflate(id, null);
            if (v == null)
                return null; // This should never be hit, but have it to clear a warning
        }
        final SettingMenuItem o = items.get(position);
        if (o != null) {
            TextView t1 = (TextView) v.findViewById(R.id.TextView01);
            TextView t2 = (TextView) v.findViewById(R.id.TextView02);

            if (t1 != null)
                t1.setText(o.getName());
            if (t2 != null)
                t2.setText(o.getSubtitle());
        }
        return v;
    }



}

