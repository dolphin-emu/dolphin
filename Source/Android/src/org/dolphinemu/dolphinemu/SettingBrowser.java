package org.dolphinemu.dolphinemu;

import android.app.ListActivity;
import android.os.Bundle;
import android.view.View;
import android.widget.ListView;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 *  Copyright 2013 Dolphin Emulator Project
 *  Licensed under GPLv2
 *  Refer to the license.txt file included.
 */

public class SettingBrowser extends ListActivity {
    private SettingMenuAdapter adapter;
    private void Fill()
    {
        this.setTitle("Settings");
        List<SettingMenuItem> dir = new ArrayList<SettingMenuItem>();

        dir.add(new SettingMenuItem("Setting 1", "SubTitle 1", 0));
        Collections.sort(dir);

        adapter = new SettingMenuAdapter(this,R.layout.folderbrowser,dir);
        this.setListAdapter(adapter);
    }

    @Override
    protected void onListItemClick(ListView l, View v, int position, long id) {
        super.onListItemClick(l, v, position, id);
        SettingMenuItem o = adapter.getItem(position);
        switch (o.getID())
        {
            default:
                // Do nothing yet
            break;
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

        Fill();
    }
    @Override
    public void onBackPressed() {
        this.finish();
        super.onBackPressed();
    }
}
