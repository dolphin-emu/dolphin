package org.dolphinemu.dolphinemu.ui.main;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.support.v17.leanback.widget.TitleViewAdapter;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.dolphinemu.dolphinemu.R;

public class CustomTitleView extends LinearLayout implements TitleViewAdapter.Provider {
    private final TextView mTitleView;
    private final View mBadgeView;

    private final TitleViewAdapter mTitleViewAdapter = new TitleViewAdapter() {
        @Override
        public View getSearchAffordanceView()
        {
            return null;
        }

        @Override
        public void setTitle(CharSequence titleText)
        {
            CustomTitleView.this.setTitle(titleText);
        }

        @Override
        public void setBadgeDrawable(Drawable drawable)
        {
            CustomTitleView.this.setBadgeDrawable(drawable);
        }
    };

    public CustomTitleView(Context context)
    {
        this(context, null);
    }

    public CustomTitleView(Context context, AttributeSet attrs)
    {
        this(context, attrs, 0);
    }

    public CustomTitleView(Context context, AttributeSet attrs, int defStyle)
    {
        super(context, attrs, defStyle);
        View root = LayoutInflater.from(context).inflate(R.layout.tv_title, this);
        mTitleView = (TextView) root.findViewById(R.id.title);
        mBadgeView = root.findViewById(R.id.badge);
    }

    public void setTitle(CharSequence title)
    {
        if (title != null)
        {
            mTitleView.setText(title);
            mTitleView.setVisibility(View.VISIBLE);
            mBadgeView.setVisibility(View.VISIBLE);
        }
    }

    public void setBadgeDrawable(Drawable drawable)
    {
        if (drawable != null)
        {
            mTitleView.setVisibility(View.GONE);
            mBadgeView.setVisibility(View.VISIBLE);
        }
    }

    @Override
    public TitleViewAdapter getTitleViewAdapter()
    {
        return mTitleViewAdapter;
    }
}