package net.simonvt.menudrawer;

import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.drawable.GradientDrawable;
import android.util.AttributeSet;
import android.view.MotionEvent;

public class LeftDrawer extends HorizontalDrawer {

    private int mIndicatorTop;

    LeftDrawer(Activity activity, int dragMode) {
        super(activity, dragMode);
    }

    public LeftDrawer(Context context) {
        super(context);
    }

    public LeftDrawer(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public LeftDrawer(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    public void openMenu(boolean animate) {
        animateOffsetTo(mMenuSize, 0, animate);
    }

    @Override
    public void closeMenu(boolean animate) {
        animateOffsetTo(0, 0, animate);
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
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        final int width = r - l;
        final int height = b - t;
        final int offsetPixels = (int) mOffsetPixels;

        mMenuContainer.layout(0, 0, mMenuSize, height);
        offsetMenu(offsetPixels);

        if (USE_TRANSLATIONS) {
            mContentContainer.layout(0, 0, width, height);
        } else {
            mContentContainer.layout(offsetPixels, 0, width + offsetPixels, height);
        }
    }

    /**
     * Offsets the menu relative to its original position based on the position of the content.
     *
     * @param offsetPixels The number of pixels the content if offset.
     */
    private void offsetMenu(int offsetPixels) {
        if (mOffsetMenu && mMenuSize != 0) {
            final int menuWidth = mMenuSize;
            final float openRatio = (menuWidth - (float) offsetPixels) / menuWidth;

            if (USE_TRANSLATIONS) {
                if (offsetPixels > 0) {
                    final int menuLeft = (int) (0.25f * (-openRatio * menuWidth));
                    mMenuContainer.setTranslationX(menuLeft);
                } else {
                    mMenuContainer.setTranslationX(-menuWidth);
                }

            } else {
                final int oldMenuLeft = mMenuContainer.getLeft();
                final int offset = (int) (0.25f * (-openRatio * menuWidth)) - oldMenuLeft;
                mMenuContainer.offsetLeftAndRight(offset);
                mMenuContainer.setVisibility(offsetPixels == 0 ? INVISIBLE : VISIBLE);
            }
        }
    }

    @Override
    protected void drawDropShadow(Canvas canvas, int offsetPixels) {
        final int height = getHeight();

        mDropShadowDrawable.setBounds(offsetPixels - mDropShadowSize, 0, offsetPixels, height);
        mDropShadowDrawable.draw(canvas);
    }

    @Override
    protected void drawMenuOverlay(Canvas canvas, int offsetPixels) {
        final int height = getHeight();
        final float openRatio = ((float) offsetPixels) / mMenuSize;

        mMenuOverlay.setBounds(0, 0, offsetPixels, height);
        mMenuOverlay.setAlpha((int) (MAX_MENU_OVERLAY_ALPHA * (1.f - openRatio)));
        mMenuOverlay.draw(canvas);
    }

    @Override
    protected void drawIndicator(Canvas canvas, int offsetPixels) {
        if (mActiveView != null && isViewDescendant(mActiveView)) {
            Integer position = (Integer) mActiveView.getTag(R.id.mdActiveViewPosition);
            final int pos = position == null ? 0 : position;

            if (pos == mActivePosition) {
                final float openRatio = ((float) offsetPixels) / mMenuSize;

                mActiveView.getDrawingRect(mActiveRect);
                offsetDescendantRectToMyCoords(mActiveView, mActiveRect);

                final float interpolatedRatio = 1.f - INDICATOR_INTERPOLATOR.getInterpolation((1.f - openRatio));
                final int interpolatedWidth = (int) (mActiveIndicator.getWidth() * interpolatedRatio);

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
                final int right = offsetPixels;
                final int left = right - interpolatedWidth;

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

    @Override
    protected void initPeekScroller() {
        final int dx = mMenuSize / 3;
        mPeekScroller.startScroll(0, 0, dx, 0, PEEK_DURATION);
    }

    @Override
    protected void onOffsetPixelsChanged(int offsetPixels) {
        if (USE_TRANSLATIONS) {
            mContentContainer.setTranslationX(offsetPixels);
            offsetMenu(offsetPixels);
            invalidate();
        } else {
            mContentContainer.offsetLeftAndRight(offsetPixels - mContentContainer.getLeft());
            offsetMenu(offsetPixels);
            invalidate();
        }
    }

    //////////////////////////////////////////////////////////////////////
    // Touch handling
    //////////////////////////////////////////////////////////////////////

    @Override
    protected boolean isContentTouch(MotionEvent ev) {
        return ev.getX() > mOffsetPixels;
    }

    @Override
    protected boolean onDownAllowDrag(MotionEvent ev) {
        return (!mMenuVisible && mInitialMotionX <= mTouchSize)
                || (mMenuVisible && mInitialMotionX >= mOffsetPixels);
    }

    @Override
    protected boolean onMoveAllowDrag(MotionEvent ev, float diff) {
        return (!mMenuVisible && mInitialMotionX <= mTouchSize && (diff > 0))
                || (mMenuVisible && mInitialMotionX >= mOffsetPixels);
    }

    @Override
    protected void onMoveEvent(float dx) {
        setOffsetPixels(Math.min(Math.max(mOffsetPixels + dx, 0), mMenuSize));
    }

    @Override
    protected void onUpEvent(MotionEvent ev) {
        final int offsetPixels = (int) mOffsetPixels;

        if (mIsDragging) {
            mVelocityTracker.computeCurrentVelocity(1000, mMaxVelocity);
            final int initialVelocity = (int) mVelocityTracker.getXVelocity();
            mLastMotionX = ev.getX();
            animateOffsetTo(mVelocityTracker.getXVelocity() > 0 ? mMenuSize : 0, initialVelocity, true);

            // Close the menu when content is clicked while the menu is visible.
        } else if (mMenuVisible && ev.getX() > offsetPixels) {
            closeMenu();
        }
    }
}
