package net.simonvt.menudrawer.samples;

import net.simonvt.menudrawer.MenuDrawer;

import android.app.ListActivity;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.view.Gravity;
import android.view.MenuItem;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import java.util.ArrayList;
import java.util.List;

public class ListActivitySample extends ListActivity {

    private static final String STATE_MENUDRAWER = "net.simonvt.menudrawer.samples.ListActivitySample.menuDrawer";

    private MenuDrawer mMenuDrawer;

    private Handler mHandler;
    private Runnable mToggleUpRunnable;
    private boolean mDisplayUp = true;

    @Override
    public void onCreate(Bundle inState) {
        super.onCreate(inState);

        mMenuDrawer = MenuDrawer.attach(this, MenuDrawer.MENU_DRAG_CONTENT);

        TextView menuView = new TextView(this);
        menuView.setGravity(Gravity.CENTER);
        menuView.setTextColor(0xFFFFFFFF);
        final int padding = dpToPx(16);
        menuView.setPadding(padding, padding, padding, padding);
        menuView.setText(R.string.sample_listactivity);
        mMenuDrawer.setMenuView(menuView);
        mMenuDrawer.setOffsetMenuEnabled(false);

        List<String> items = new ArrayList<String>();
        for (int i = 1; i <= 20; i++) {
            items.add("Item " + i);
        }

        setListAdapter(new ArrayAdapter<String>(this, android.R.layout.simple_list_item_1, items));
    }

    private int dpToPx(int dp) {
        return (int) (getResources().getDisplayMetrics().density * dp + 0.5f);
    }

    @Override
    public void setContentView(int layoutResID) {
        // This override is only needed when using MENU_DRAG_CONTENT.
        mMenuDrawer.setContentView(layoutResID);
        onContentChanged();
    }

    @Override
    protected void onListItemClick(ListView l, View v, int position, long id) {
        String str = (String) getListAdapter().getItem(position);
        Toast.makeText(this, "Clicked: " + str, Toast.LENGTH_SHORT).show();
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case android.R.id.home:
                mMenuDrawer.toggleMenu();
                return true;
        }

        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onBackPressed() {
        final int drawerState = mMenuDrawer.getDrawerState();
        if (drawerState == MenuDrawer.STATE_OPEN || drawerState == MenuDrawer.STATE_OPENING) {
            mMenuDrawer.closeMenu();
            return;
        }

        super.onBackPressed();
    }
}
