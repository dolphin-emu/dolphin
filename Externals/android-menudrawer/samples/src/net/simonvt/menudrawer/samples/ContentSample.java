package net.simonvt.menudrawer.samples;

import net.simonvt.menudrawer.MenuDrawer;

import android.app.Activity;
import android.os.Build;
import android.os.Bundle;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.ListView;
import android.widget.SeekBar;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

public class ContentSample extends Activity {

    private static final String STATE_ACTIVE_POSITION = "net.simonvt.menudrawer.samples.ContentSample.activePosition";
    private static final String STATE_CONTENT_TEXT = "net.simonvt.menudrawer.samples.ContentSample.contentText";

    private MenuDrawer mMenuDrawer;

    private MenuAdapter mAdapter;
    private ListView mList;

    private int mActivePosition = -1;
    private String mContentText;
    private TextView mContentTextView;

    @Override
    protected void onCreate(Bundle inState) {
        super.onCreate(inState);

        if (inState != null) {
            mActivePosition = inState.getInt(STATE_ACTIVE_POSITION);
            mContentText = inState.getString(STATE_CONTENT_TEXT);
        }

        mMenuDrawer = MenuDrawer.attach(this, MenuDrawer.MENU_DRAG_CONTENT);
        mMenuDrawer.setContentView(R.layout.activity_contentsample);
        mMenuDrawer.setTouchMode(MenuDrawer.TOUCH_MODE_FULLSCREEN);

        List<Object> items = new ArrayList<Object>();
        items.add(new Item("Item 1", R.drawable.ic_action_refresh_dark));
        items.add(new Item("Item 2", R.drawable.ic_action_select_all_dark));
        items.add(new Category("Cat 1"));
        items.add(new Item("Item 3", R.drawable.ic_action_refresh_dark));
        items.add(new Item("Item 4", R.drawable.ic_action_select_all_dark));
        items.add(new Category("Cat 2"));
        items.add(new Item("Item 5", R.drawable.ic_action_refresh_dark));
        items.add(new Item("Item 6", R.drawable.ic_action_select_all_dark));
        items.add(new Category("Cat 3"));
        items.add(new Item("Item 7", R.drawable.ic_action_refresh_dark));
        items.add(new Item("Item 8", R.drawable.ic_action_select_all_dark));
        items.add(new Category("Cat 4"));
        items.add(new Item("Item 9", R.drawable.ic_action_refresh_dark));
        items.add(new Item("Item 10", R.drawable.ic_action_select_all_dark));

        mList = new ListView(this);
        mAdapter = new MenuAdapter(items);
        mList.setAdapter(mAdapter);
        mList.setOnItemClickListener(mItemClickListener);

        mMenuDrawer.setMenuView(mList);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.ICE_CREAM_SANDWICH) {
            getActionBar().setDisplayHomeAsUpEnabled(true);
        }

        mContentTextView = (TextView) findViewById(R.id.contentText);
        mContentTextView.setText(mContentText);

        mMenuDrawer.setOnInterceptMoveEventListener(new MenuDrawer.OnInterceptMoveEventListener() {
            @Override
            public boolean isViewDraggable(View v, int dx, int x, int y) {
                return v instanceof SeekBar;
            }
        });
    }

    private AdapterView.OnItemClickListener mItemClickListener = new AdapterView.OnItemClickListener() {
        @Override
        public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
            mActivePosition = position;
            mMenuDrawer.setActiveView(view, position);
            mContentTextView.setText(((TextView) view).getText());
            mMenuDrawer.closeMenu();
        }
    };

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putInt(STATE_ACTIVE_POSITION, mActivePosition);
        outState.putString(STATE_CONTENT_TEXT, mContentText);
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

    private static class Item {

        String mTitle;
        int mIconRes;

        Item(String title, int iconRes) {
            mTitle = title;
            mIconRes = iconRes;
        }
    }

    private static class Category {

        String mTitle;

        Category(String title) {
            mTitle = title;
        }
    }

    private class MenuAdapter extends BaseAdapter {

        private List<Object> mItems;

        MenuAdapter(List<Object> items) {
            mItems = items;
        }

        @Override
        public int getCount() {
            return mItems.size();
        }

        @Override
        public Object getItem(int position) {
            return mItems.get(position);
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public int getItemViewType(int position) {
            return getItem(position) instanceof Item ? 0 : 1;
        }

        @Override
        public int getViewTypeCount() {
            return 2;
        }

        @Override
        public boolean isEnabled(int position) {
            return getItem(position) instanceof Item;
        }

        @Override
        public boolean areAllItemsEnabled() {
            return false;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            View v = convertView;
            Object item = getItem(position);

            if (item instanceof Category) {
                if (v == null) {
                    v = getLayoutInflater().inflate(R.layout.menu_row_category, parent, false);
                }

                ((TextView) v).setText(((Category) item).mTitle);

            } else {
                if (v == null) {
                    v = getLayoutInflater().inflate(R.layout.menu_row_item, parent, false);
                }

                TextView tv = (TextView) v;
                tv.setText(((Item) item).mTitle);
                tv.setCompoundDrawablesWithIntrinsicBounds(((Item) item).mIconRes, 0, 0, 0);
            }

            v.setTag(R.id.mdActiveViewPosition, position);

            if (position == mActivePosition) {
                mMenuDrawer.setActiveView(v, position);
            }

            return v;
        }
    }
}
