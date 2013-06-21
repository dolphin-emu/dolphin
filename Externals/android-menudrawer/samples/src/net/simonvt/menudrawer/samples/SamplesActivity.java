package net.simonvt.menudrawer.samples;

import android.app.ListActivity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.ListView;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

public class SamplesActivity extends ListActivity {

    private SamplesAdapter mAdapter;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mAdapter = new SamplesAdapter();

        mAdapter.addSample("Content sample", "Only the content area is dragged.", ContentSample.class);
        mAdapter.addSample("ListActivity sample", "Shows how to use the drawer with a ListActivity.",
                ListActivitySample.class);
        mAdapter.addSample("Window sample", "The entire window is dragged.", WindowSample.class);
        mAdapter.addSample("ActionBar overlay sample", "A window sample, where the ActionBar is an overlay",
                ActionBarOverlaySample.class);
        mAdapter.addSample("Right menu", "The menu is positioned to the right of the content", RightMenuSample.class);
        mAdapter.addSample("Top menu", "The menu is positioned above the content", TopMenuSample.class);
        mAdapter.addSample("Bottom menu", "The menu is positioned below the content", BottomMenuSample.class);
        mAdapter.addSample("Touch Mode", "The menu touch behavior change according to different"
                + " content view state (Ex: View Pager)", ViewPagerSample.class);
        mAdapter.addSample("Layout xml", "The drawer and its menu and content is defined in XML", LayoutSample.class);
        mAdapter.addSample("Static drawer", "The drawer is always visible", StaticDrawerSample.class);

        setListAdapter(mAdapter);
    }

    @Override
    protected void onListItemClick(ListView l, View v, int position, long id) {
        SampleItem sample = (SampleItem) mAdapter.getItem(position);
        Intent i = new Intent(this, sample.mClazz);
        startActivity(i);
    }

    private static class SampleItem {

        String mTitle;
        String mSummary;
        Class mClazz;

        public SampleItem(String title, String summary, Class clazz) {
            mTitle = title;
            mSummary = summary;
            mClazz = clazz;
        }
    }

    public class SamplesAdapter extends BaseAdapter {

        private List<SampleItem> mSamples = new ArrayList<SampleItem>();

        public void addSample(String title, String summary, Class clazz) {
            mSamples.add(new SampleItem(title, summary, clazz));
        }

        @Override
        public int getCount() {
            return mSamples.size();
        }

        @Override
        public Object getItem(int position) {
            return mSamples.get(position);
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            SampleItem sample = (SampleItem) getItem(position);

            View v = convertView;
            if (v == null) {
                v = getLayoutInflater().inflate(R.layout.list_row_sample, parent, false);
            }

            ((TextView) v.findViewById(R.id.title)).setText(sample.mTitle);
            ((TextView) v.findViewById(R.id.summary)).setText(sample.mSummary);

            return v;
        }
    }
}
