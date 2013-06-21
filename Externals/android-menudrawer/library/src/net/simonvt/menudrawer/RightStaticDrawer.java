package net.simonvt.menudrawer;

import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.drawable.GradientDrawable;
import android.util.AttributeSet;

public class RightStaticDrawer extends StaticDrawer {

    private int mIndicatorTop;

    RightStaticDrawer(Activity activity, int dragMode) {
        super(activity, dragMode);
    }

    public RightStaticDrawer(Context context) {
        super(context);
    }

    public RightStaticDrawer(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public RightStaticDrawer(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    protected void initDrawer(Context context, AttributeSet attrs, int defStyle) {
        super.initDrawer(context, attrs, defStyle);
        mPosition = Position.RIGHT;
    }

    @Override
    public void setDropShadowColor(int color) {
        final int endColor = color & 0x00FFFFFF;
        mDropShadowDrawable = new GradientDrawable(GradientDrawable.Orientation.LEFT_RIGHT, new int[] {
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
                final int width = getWidth();
                final int menuWidth = mMenuSize;
                final int indicatorWidth = mActiveIndicator.getWidth();

                final int contentRight = width - menuWidth;

                mActiveView.getDrawingRect(mActiveRect);
                offsetDescendantRectToMyCoords(mActiveView, mActiveRect);

                final int indicatorRight = contentRight + indicatorWidth;
                final int indicatorLeft = contentRight;

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

                canvas.save();
                canvas.clipRect(contentRight, 0, indicatorRight, getHeight());
                canvas.drawBitmap(mActiveIndicator, indicatorLeft, mIndicatorTop, null);
                canvas.restore();
            }
        }
    }

    @Override
    protected int getIndicatorStartPos() {
        return mIndicatorTop;
    }
}
