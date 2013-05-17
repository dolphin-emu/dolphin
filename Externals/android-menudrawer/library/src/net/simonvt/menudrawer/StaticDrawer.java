package net.simonvt.menudrawer;

import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.util.AttributeSet;

public abstract class StaticDrawer extends MenuDrawer {

    protected Position mPosition;

    StaticDrawer(Activity activity, int dragMode) {
        super(activity, dragMode);
    }

    public StaticDrawer(Context context) {
        super(context);
    }

    public StaticDrawer(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public StaticDrawer(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    protected void dispatchDraw(Canvas canvas) {
        super.dispatchDraw(canvas);
        if (mDropShadowEnabled) drawDropShadow(canvas);
        if (mActiveIndicator != null) drawIndicator(canvas);
    }

    private void drawDropShadow(Canvas canvas) {
        final int width = getWidth();
        final int height = getHeight();
        final int menuSize = mMenuSize;
        final int dropShadowSize = mDropShadowSize;

        switch (mPosition) {
            case LEFT:
                mDropShadowDrawable.setBounds(menuSize - dropShadowSize, 0, menuSize, height);
                break;

            case TOP:
                mDropShadowDrawable.setBounds(0, menuSize - dropShadowSize, width, menuSize);
                break;

            case RIGHT:
                mDropShadowDrawable.setBounds(width - menuSize, 0, width - menuSize + dropShadowSize, height);
                break;

            case BOTTOM:
                mDropShadowDrawable.setBounds(0, height - menuSize, width, height - menuSize + dropShadowSize);
                break;
        }

        mDropShadowDrawable.draw(canvas);
    }

    protected abstract void drawIndicator(Canvas canvas);

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        final int width = r - l;
        final int height = b - t;

        switch (mPosition) {
            case LEFT:
                mMenuContainer.layout(0, 0, mMenuSize, height);
                mContentContainer.layout(mMenuSize, 0, width, height);
                break;

            case RIGHT:
                mMenuContainer.layout(width - mMenuSize, 0, width, height);
                mContentContainer.layout(0, 0, width - mMenuSize, height);
                break;

            case TOP:
                mMenuContainer.layout(0, 0, width, mMenuSize);
                mContentContainer.layout(0, mMenuSize, width, height);
                break;

            case BOTTOM:
                mMenuContainer.layout(0, height - mMenuSize, width, height);
                mContentContainer.layout(0, 0, width, height - mMenuSize);
                break;
        }
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        final int widthMode = MeasureSpec.getMode(widthMeasureSpec);
        final int heightMode = MeasureSpec.getMode(heightMeasureSpec);

        if (widthMode != MeasureSpec.EXACTLY || heightMode != MeasureSpec.EXACTLY) {
            throw new IllegalStateException("Must measure with an exact size");
        }

        final int width = MeasureSpec.getSize(widthMeasureSpec);
        final int height = MeasureSpec.getSize(heightMeasureSpec);

        if (!mMenuSizeSet) mMenuSize = (int) (height * 0.25f);

        switch (mPosition) {
            case LEFT:
            case RIGHT: {
                final int childHeightMeasureSpec = MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY);

                final int menuWidth = mMenuSize;
                final int menuWidthMeasureSpec = MeasureSpec.makeMeasureSpec(menuWidth, MeasureSpec.EXACTLY);

                final int contentWidth = width - menuWidth;
                final int contentWidthMeasureSpec = MeasureSpec.makeMeasureSpec(contentWidth, MeasureSpec.EXACTLY);

                mContentContainer.measure(contentWidthMeasureSpec, childHeightMeasureSpec);
                mMenuContainer.measure(menuWidthMeasureSpec, childHeightMeasureSpec);
                break;
            }

            case TOP:
            case BOTTOM: {
                final int childWidthMeasureSpec = MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY);

                final int menuHeight = mMenuSize;
                final int menuHeightMeasureSpec = MeasureSpec.makeMeasureSpec(menuHeight, MeasureSpec.EXACTLY);

                final int contentHeight = height - menuHeight;
                final int contentHeightMeasureSpec = MeasureSpec.makeMeasureSpec(contentHeight, MeasureSpec.EXACTLY);

                mContentContainer.measure(childWidthMeasureSpec, contentHeightMeasureSpec);
                mMenuContainer.measure(childWidthMeasureSpec, menuHeightMeasureSpec);
                break;
            }
        }

        setMeasuredDimension(width, height);
    }

    @Override
    public void toggleMenu(boolean animate) {
    }

    @Override
    public void openMenu(boolean animate) {
    }

    @Override
    public void closeMenu(boolean animate) {
    }

    @Override
    public boolean isMenuVisible() {
        return true;
    }

    @Override
    public void setMenuSize(int size) {
        mMenuSize = size;
        mMenuSizeSet = true;
        requestLayout();
        invalidate();
    }

    @Override
    public void setOffsetMenuEnabled(boolean offsetMenu) {
    }

    @Override
    public boolean getOffsetMenuEnabled() {
        return false;
    }

    @Override
    public void peekDrawer() {
    }

    @Override
    public void peekDrawer(long delay) {
    }

    @Override
    public void peekDrawer(long startDelay, long delay) {
    }

    @Override
    public void setHardwareLayerEnabled(boolean enabled) {
    }

    @Override
    public int getTouchMode() {
        return TOUCH_MODE_NONE;
    }

    @Override
    public void setTouchMode(int mode) {
    }

    @Override
    public void setTouchBezelSize(int size) {
    }

    @Override
    public int getTouchBezelSize() {
        return 0;
    }
}
