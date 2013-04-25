package net.simonvt.menudrawer.samples;

import net.simonvt.menudrawer.MenuDrawer;
import net.simonvt.menudrawer.Position;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.TextView;

/**
 * Sample class illustrating how to add a menu drawer above the content area.
 */
public class BottomMenuSample extends Activity implements OnClickListener {

    private MenuDrawer mMenuDrawer;
    private TextView mContentTextView;

    @Override
    protected void onCreate(Bundle inState) {
        super.onCreate(inState);

        mMenuDrawer = MenuDrawer.attach(this, MenuDrawer.MENU_DRAG_CONTENT, Position.BOTTOM);
        mMenuDrawer.setTouchMode(MenuDrawer.TOUCH_MODE_FULLSCREEN);
        mMenuDrawer.setContentView(R.layout.activity_bottommenu);
        mMenuDrawer.setMenuView(R.layout.menu_bottom);

        mContentTextView = (TextView) findViewById(R.id.contentText);
        findViewById(R.id.item1).setOnClickListener(this);
        findViewById(R.id.item2).setOnClickListener(this);
    }

    /**
     * Click handler for bottom drawer items.
     */
    @Override
    public void onClick(View v) {
        String tag = (String) v.getTag();
        mContentTextView.setText(String.format("%s clicked.", tag));
        mMenuDrawer.setActiveView(v);
    }
}
