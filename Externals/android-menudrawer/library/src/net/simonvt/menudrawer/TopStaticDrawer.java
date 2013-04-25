package net.simonvt.menudrawer;

import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.drawable.GradientDrawable;
import android.util.AttributeSet;

public class TopStaticDrawer extends StaticDrawer {

    private int mIndicatorLeft;

    TopStaticDrawer(Activity activity, int dragMode) {
        super(activity, dragMode);
    }

    public TopStaticDrawer(Context context) {
        super(context);
    }

    public TopStaticDrawer(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public TopStaticDrawer(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    protected void initDrawer(Context context, AttributeSet attrs, int defStyle) {
        super.initDrawer(context, attrs, defStyle);
        mPosition = Position.TOP;
    }

    @Override
    public void setDropShadowColor(int color) {
        final int endColor = color & 0x00FFFFFF;
        mDropShadowDrawable = new GradientDrawable(GradientDrawable.Orientation.BOTTOM_TOP, new int[] {
                color,
                endColor,
        });
        invalidate();
    }

    @Override
    protected void drawIndicator(Canvas canvas) {
        if (mActiveView != null && isViewDescendant(mActiveView)) {
            Integer position = (Integer) mActiveView.getTag(R.id.mdActiveViewPosition);
            final int pos = position == null ? 0 : position;

            if (pos == mActivePosition) {
                final int menuHeight = mMenuSize;
                final int indicatorHeight = mActiveIndicator.getHeight();

                mActiveView.getDrawingRect(mActiveRect);
                offsetDescendantRectToMyCoords(mActiveView, mActiveRect);
                final int indicatorWidth = mActiveIndicator.getWidth();

                final int indicatorTop = menuHeight - indicatorHeight;
                if (mIndicatorAnimating) {
                    final int finalLeft = mActiveRect.left + ((mActiveRect.width() - indicatorWidth) / 2);
                    final int startLeft = mIndicatorStartPos;
                    final int diff = finalLeft - startLeft;
                    final int startOffset = (int) (diff * mIndicatorOffset);
                    mIndicatorLeft = startLeft + startOffset;
                } else {
                    mIndicatorLeft = mActiveRect.left + ((mActiveRect.width() - indicatorWidth) / 2);
                }

                canvas.save();
                canvas.clipRect(mIndicatorLeft, indicatorTop, mIndicatorLeft + indicatorWidth, menuHeight);
                canvas.drawBitmap(mActiveIndicator, mIndicatorLeft, indicatorTop, null);
                canvas.restore();
            }
        }
    }

    @Override
    protected int getIndicatorStartPos() {
        return mIndicatorLeft;
    }
}
