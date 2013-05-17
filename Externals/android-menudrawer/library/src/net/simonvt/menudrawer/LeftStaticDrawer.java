package net.simonvt.menudrawer;

import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.drawable.GradientDrawable;
import android.util.AttributeSet;

public class LeftStaticDrawer extends StaticDrawer {

    private int mIndicatorTop;

    LeftStaticDrawer(Activity activity, int dragMode) {
        super(activity, dragMode);
    }

    public LeftStaticDrawer(Context context) {
        super(context);
    }

    public LeftStaticDrawer(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public LeftStaticDrawer(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    protected void initDrawer(Context context, AttributeSet attrs, int defStyle) {
        super.initDrawer(context, attrs, defStyle);
        mPosition = Position.LEFT;
    }

    @Override
    public void setDropShadowColor(int color) {
        final int endColor = color & 0x00FFFFFF;
        mDropShadowDrawable = new GradientDrawable(GradientDrawable.Orientation.RIGHT_LEFT, new int[] {
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
                mActiveView.getDrawingRect(mActiveRect);
                offsetDescendantRectToMyCoords(mActiveView, mActiveRect);

                if (mIndicatorAnimating) {
                    final int indicatorFinalTop = mActiveRect.top + ((mActiveRect.height()
                            - mActiveIndicator.getHeight()) / 2);
                    final int indicatorStartTop = mIndicatorStartPos;
                    final int diff = indicatorFinalTop - indicatorStartTop;
                    final int startOffset = (int) (diff * mIndicatorOffset);
                    mIndicatorTop = indicatorStartTop + startOffset;
                } else {
                    mIndicatorTop = mActiveRect.top + ((mActiveRect.height() - mActiveIndicator.getHeight()) / 2);
                }
                final int right = mMenuSize;
                final int left = right - mActiveIndicator.getWidth();

                canvas.save();
                canvas.clipRect(left, 0, right, getHeight());
                canvas.drawBitmap(mActiveIndicator, left, mIndicatorTop, null);
                canvas.restore();
            }
        }
    }

    @Override
    protected int getIndicatorStartPos() {
        return mIndicatorTop;
    }
}
